#!/bin/bash
# Extract all GhostLock offsets from vmlinux and generate target.h values
# Usage: ./extract_offsets.sh /path/to/vmlinux

VMLINUX="${1:-vmlinux}"
BASE=0xffffffc080000000

if [ ! -f "$VMLINUX" ]; then
    echo "Usage: $0 <path/to/vmlinux>"
    exit 1
fi

echo "=== EXTRACTING OFFSETS FROM $VMLINUX ==="
echo "Base: $BASE"
echo ""

# Extract all needed symbols
nm "$VMLINUX" | grep -E \
    ' (T|t|D|d|B|b) (ashmem_ioctl|compat_ashmem_ioctl|ashmem_mmap|ashmem_open|ashmem_release|ashmem_show_fdinfo|ashmem_misc|ashmem_fops|configfs_read_iter|configfs_bin_attr_write_iter|copy_splice_read|noop_llseek|init_task|root_task_group|selinux_enforcing|kmalloc_caches|anon_pipe_buf_ops|system_unbound_wq|call_usermodehelper_exec_work|nfulnl_logger|nf_loggers|boot_id|sysctl_bootid|process_one_work)$' \
    | while read addr type name; do
        offset=$(python3 -c "print('0x%08XULL' % (0x$addr - $BASE))")
        printf "%-45s %s  /* nm: %s %s @ 0x%s */\n" "#define ${name^^}_OFF" "$offset" "$type" "$name" "$addr"
    done

echo ""
echo "=== PASTE ABOVE INTO target.h ==="
echo "Replace the PENDING values with these offsets"
