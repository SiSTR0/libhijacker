#include "main.h"

extern "C" {
	#include <ps5/payload_main.h>
	#include <arpa/inet.h>
	#include <netinet/in.h>
	#include <sys/socket.h>
	#include <string.h>
	#include <stdio.h>
	#include <unistd.h>
	#include <stdarg.h>
	ssize_t _write(int, const void *, size_t);
	void kernel_copyin(const void *src, uint64_t kdest, size_t length);
	void kernel_copyout(uint64_t ksrc, void *dest, size_t length);
	extern uintptr_t kernel_base;
}

void printf_notification(const char *fmt, ...) {
	SceNotificationRequest noti_buffer;

	va_list args;
	va_start(args, fmt);
	vsprintf(noti_buffer.message, fmt, args);
	va_end(args);

	noti_buffer.type = 0;
	noti_buffer.unk3 = 0;
	noti_buffer.use_icon_image_uri = 1;
	noti_buffer.target_id = -1;
	strcpy(noti_buffer.uri, "cxml://psnotification/tex_icon_system");

	sceKernelSendNotificationRequest(0, (SceNotificationRequest * ) & noti_buffer, sizeof(noti_buffer), 0);
}

class Buffer {
	uint8_t *buf;

	public:
		explicit Buffer() : buf(new uint8_t[BUFSIZ]) {}
		~Buffer() { delete[] buf; }
		constexpr size_t length() const { return BUFSIZ; }
		operator void*() const { return buf; }
};

class Socket {
	int fd;

	void close() {
		if (fd != -1) {
			::close(fd);
			fd = -1;
		}
	}

	public:
		Socket() : fd(-1) {}
		Socket(int fd) : fd(fd) {}
		Socket(const Socket&) = delete;
		Socket(Socket &&rhs) : fd(rhs.fd) { rhs.fd = -1; }
		Socket &operator=(int fd) {
			close();
			this->fd = fd;
			return *this;
		}
		Socket &operator=(const Socket &rhs) = delete;
		Socket &operator=(Socket &&rhs) {
			close();
			fd = rhs.fd;
			rhs.fd = -1;
			return *this;
		}
		~Socket() {
			close();
		}
		int getFd() const { return fd; }
		explicit operator bool() const { return fd != -1; }
		void write(const Buffer &buf) const {
			_write(fd, buf, buf.length());
		}
};

Socket connect() {
	socklen_t addr_len;
	int sock = socket(AF_INET, SOCK_STREAM, 0);

	if (sock == -1) {
		return {};
	}

	int value = 1;
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) < 0) {
		return {};
	}

	struct sockaddr_in server_addr{0, AF_INET, htons(9030), {}, {}};

	if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
		return {};
	}

	if (listen(sock, 1) != 0) {
		return {};
	}

	struct sockaddr client_addr{};
	addr_len = sizeof(client_addr);
	int fd = accept(sock, &client_addr, &addr_len);
	close(sock);
	return fd;
}

constexpr size_t KERNEL_DATA_SIZE = 0x87BF000;

int main() {
	printf_notification("Hello by test_elf.elf");
	Buffer buf{};
	Socket sock = connect();
	dup2(sock.getFd(), 1);
	dup2(sock.getFd(), 2);
	for (size_t i = 0; i < KERNEL_DATA_SIZE; i += buf.length()) {
		kernel_copyout(kernel_base + i, buf, buf.length());
		sock.write(buf);
	}
	return 0;
}
