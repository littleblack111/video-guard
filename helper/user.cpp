#include "kernel.skel.h"
#include <bpf/bpf.h>
#include <bpf/libbpf.h>
#include <cstdlib>
#include <cstring>
#include <print>
#include <sys/resource.h>
#include <unistd.h>

#define MAX_PATH_LEN 256

struct event {
	uint32_t pid;
};

static void bump_memlock_rlimit() {
	struct rlimit r = {RLIM_INFINITY, RLIM_INFINITY};
	if (setrlimit(RLIMIT_MEMLOCK, &r)) {
		perror("setrlimit(RLIMIT_MEMLOCK)");
		std::exit(1);
	}
}

static int handle_event(void *ctx, void *data, size_t) {
	auto *e = static_cast<event *>(data);
	std::println("{}", e->pid);
	std::exit(0);
	return 0;
}

int main(int argc, char **argv) {
	if (argc != 2) {
		std::println(stderr, "Usage: {} <video_index>", argv[0]);
		return 1;
	}
	int idx = std::atoi(argv[1]);
	char target[MAX_PATH_LEN];
	std::snprintf(target, sizeof(target), "/dev/video%d", idx);

	bump_memlock_rlimit();

	struct kernel_bpf *skel = kernel_bpf__open_and_load();
	if (!skel) {
		std::println(stderr, "ERROR: opening BPF skeleton");
		return 1;
	}

	uint32_t key = 0;
	if (bpf_map_update_elem(bpf_map__fd(skel->maps.target_path), &key, target,
							BPF_ANY)) {
		perror("bpf_map_update_elem");
		kernel_bpf__destroy(skel);
		return 1;
	}

	if (kernel_bpf__attach(skel)) {
		std::println(stderr, "ERROR: attaching BPF programs");
		kernel_bpf__destroy(skel);
		return 1;
	}

	int map_fd = bpf_map__fd(skel->maps.events);
	struct ring_buffer *rb =
		ring_buffer__new(map_fd, handle_event, nullptr, nullptr);
	if (!rb) {
		std::println(stderr, "ERROR: creating ring buffer");
		kernel_bpf__destroy(skel);
		return 1;
	}

	while (true) {
		int err = ring_buffer__poll(rb, 100 /* ms */);
		if (err < 0 && err != -EINTR) {
			std::println(stderr, "ERROR: polling ring buffer: {}", err);
			break;
		}
	}

	ring_buffer__free(rb);
	kernel_bpf__destroy(skel);
	return 0;
}
