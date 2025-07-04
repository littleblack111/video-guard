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

struct {
	__uint(type, BPF_MAP_TYPE_ARRAY);
	__uint(max_entries, 1);
	__type(key, u32);
	__type(value, u32);
} mode_map SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, u64);
	__type(value, u8);
} open_pids SEC(".maps");

struct pidfd {
	u32 pid;
	u32 fd;
};

struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, 1024);
	__type(key, struct pidfd);
	__type(value, u8);
} fd_map SEC(".maps");

struct event_t {
	u32 pid;
};

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 1 << 24);
} events SEC(".maps");

SEC("tracepoint/syscalls/sys_enter_openat")
int trace_enter_openat(struct trace_event_raw_sys_enter *ctx) {
	u32 key = 0;
	u32 *mode = bpf_map_lookup_elem(&mode_map, &key);
	if (!mode)
		return 0;
	char *target = bpf_map_lookup_elem(&target_path, &key);
	if (!target)
		return 0;
	char filename[MAX_PATH_LEN];
	int len = bpf_probe_read_str(filename, sizeof(filename),
								 (const void *)ctx->args[1]);
	if (len <= 0)
		return 0;
#pragma unroll
	for (int i = 0; i < MAX_PATH_LEN; i++) {
		if (filename[i] != target[i])
			return 0;
		if (filename[i] == '\0')
			break;
	}
	if (*mode == 0) {
		struct event_t evt = {};
		u64 tg = bpf_get_current_pid_tgid();
		evt.pid = tg >> 32;
		bpf_ringbuf_output(&events, &evt, sizeof(evt), 0);
	} else {
		u64 pid_tgid = bpf_get_current_pid_tgid();
		u8 flag = 1;
		bpf_map_update_elem(&open_pids, &pid_tgid, &flag, BPF_ANY);
	}
	return 0;
}

SEC("tracepoint/syscalls/sys_exit_openat")
int trace_exit_openat(struct trace_event_raw_sys_exit *ctx) {
	u32 key = 0;
	u32 *mode = bpf_map_lookup_elem(&mode_map, &key);
	if (!mode || *mode != 1)
		return 0;
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u8 *flag = bpf_map_lookup_elem(&open_pids, &pid_tgid);
	if (!flag)
		return 0;
	bpf_map_delete_elem(&open_pids, &pid_tgid);
	int fd = ctx->ret;
	if (fd < 0)
		return 0;
	struct pidfd k = {.pid = (u32)(pid_tgid >> 32), .fd = (u32)fd};
	u8 one = 1;
	bpf_map_update_elem(&fd_map, &k, &one, BPF_ANY);
	return 0;
}

SEC("tracepoint/syscalls/sys_enter_close")
int trace_enter_close(struct trace_event_raw_sys_enter *ctx) {
	u32 key = 0;
	u32 *mode = bpf_map_lookup_elem(&mode_map, &key);
	if (!mode || *mode != 1)
		return 0;
	u64 pid_tgid = bpf_get_current_pid_tgid();
	u32 pid = pid_tgid >> 32;
	u32 fd = (u32)ctx->args[0];
	struct pidfd k = {.pid = pid, .fd = fd};
	u8 *flag = bpf_map_lookup_elem(&fd_map, &k);
	if (!flag)
		return 0;
	bpf_map_delete_elem(&fd_map, &k);
	struct event_t evt = {.pid = pid};
	bpf_ringbuf_output(&events, &evt, sizeof(evt), 0);
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
