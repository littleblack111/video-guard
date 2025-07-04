#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#define MAX_PATH_LEN 256

#define u32 __u32
#define u64 __u64

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, char[MAX_PATH_LEN]);
} target_path SEC(".maps");

struct event_t {
	u32 pid;
};

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 24);
} events SEC(".maps");

SEC("tracepoint/syscalls/sys_enter_openat")
int trace_openat(struct trace_event_raw_sys_enter *ctx) {
	u32 key = 0;
	char *target = bpf_map_lookup_elem(&target_path, &key);
	if (!target)
		return 0;

	char filename[MAX_PATH_LEN];
	int len = bpf_probe_read_str(filename, sizeof(filename),
								 (const void *)ctx->args[1]);
	if (len <= 0)
		return 0;

/* compare filename == target */
#pragma unroll
	for (int i = 0; i < MAX_PATH_LEN; i++) {
		if (filename[i] != target[i])
			return 0;
		if (filename[i] == '\0')
			break;
	}

	struct event_t evt = {};
	u64 tg = bpf_get_current_pid_tgid();
	evt.pid = tg >> 32;

	bpf_ringbuf_output(&events, &evt, sizeof(evt), 0);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
