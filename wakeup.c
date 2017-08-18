#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/epoll.h>

#define FD_UNINIT (-1)
#define PIPE_READ_FD(pfd) (pfd[0])
#define PIPE_WRITE_FD(pfd) (pfd[1])
struct wakeup {
	int epfd;
	int pipefd[2];
};

static void *sleeper(void *ctx)
{
	struct wakeup *w = (struct wakeup*)ctx;
	struct epoll_event ev;
	int idx = 0;
	char data;

	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = PIPE_READ_FD(w->pipefd);
	epoll_ctl(w->epfd, EPOLL_CTL_ADD, PIPE_READ_FD(w->pipefd), &ev);

	while (1) {
		int num;

		num = epoll_wait(w->epfd, &ev, 1, -1);
		read(PIPE_READ_FD(w->pipefd), &data, 1);
		printf("%d. Got %d events (%c)\n", ++idx, num, data);
		fflush(stdout);
	}

	return NULL;
}

int main(int argc, char **argv)
{
	int ret;
	struct wakeup w;
	struct epoll_event ev;
	pthread_t tid;
	char trigger = 'x';
	char *line = NULL;
	size_t len = 0;

	PIPE_READ_FD(w.pipefd) = FD_UNINIT;
	PIPE_WRITE_FD(w.pipefd) = FD_UNINIT;

	ret = pipe(w.pipefd);
	if (ret) {
		printf("Failed to create a wakeup pipe, Aborting...\n");
		return -1;
	}

	w.epfd = epoll_create1(0);
	if (w.epfd == -1) {
		printf("Failed to create epoll, Aborting...\n");
		ret = -1;
		goto exit;
	}

	ev.events = EPOLLOUT | EPOLLET;
	ev.data.fd = PIPE_WRITE_FD(w.pipefd);
	epoll_ctl(w.epfd, EPOLL_CTL_ADD, PIPE_WRITE_FD(w.pipefd), &ev);

	ret = pthread_create(&tid, NULL, sleeper, &w);
	if (ret) {
		printf("Failed to create sleeper thread. Aborting...\n");
		goto exit;
	}

	printf("Press 't' to trigger the sleeper or 'x' to exit\n");
	while (1) {
		printf("> ");
		fflush(stdout);
		getline(&line, &len, stdin);

		if (!line)
			break;

		if (*line == 't') {
			printf("Triggering sleeper...\n");
			fflush(stdout);
			write(PIPE_WRITE_FD(w.pipefd), &trigger, 1);
			usleep(1);
		}
		if (*line == 'x') {
			printf("Terminating...\n");
			fflush(stdout);
			break;
		}
	}

exit:
	free(line);
	close(PIPE_READ_FD(w.pipefd));
	close(PIPE_WRITE_FD(w.pipefd));
	return ret;
}

