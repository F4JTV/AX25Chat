/*
 * dwcore_test - console harness for the embedded Dire Wolf core.
 *
 * Starts the core with a direwolf.conf, prints every decoded frame, DCD and
 * PTT changes and the core's log, and can queue a test UI frame.  Runs for a
 * fixed time or until Ctrl-C, then stops the core cleanly and, with --twice,
 * starts it again to prove that the stop/start path works.
 *
 *   dwcore_test -c direwolf.conf [-t seconds] [-s "SRC>DST:text"] [--twice]
 *
 * With "ADEVICE stdin null" in the configuration file, raw audio can be piped
 * in, for example the output of gen_packets from the Direwolf tools:
 *
 *   gen_packets -o test.wav && dwcore_test -c conf/test-stdin.conf < test.wav
 *
 * No Qt here on purpose: this is the smallest program that exercises the C
 * core, and it builds on any machine with a C++17 compiler.
 *
 * This file is part of AX25Chat.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "dw_embed.h"

namespace {

std::atomic<bool> g_quit{false};
std::atomic<int> g_frames{0};

const char *level_name(dw_embed_log_level_t level)
{
	switch (level) {
	  case DW_EMBED_LOG_ERROR:   return "ERROR";
	  case DW_EMBED_LOG_REC:     return "REC";
	  case DW_EMBED_LOG_DECODED: return "DECODED";
	  case DW_EMBED_LOG_XMIT:    return "XMIT";
	  case DW_EMBED_LOG_DEBUG:   return "DEBUG";
	  default:                   return "INFO";
	}
}

void on_log(void *, dw_embed_log_level_t level, const char *line)
{
	std::printf("[dw %-7s] %s\n", level_name(level), line);
	std::fflush(stdout);
}

void on_frame(void *, const dw_embed_frame_t *f)
{
	char addrs[200];
	dw_embed_format_addrs(f->data, f->len, addrs, sizeof(addrs));

	/* Information part: skip addresses (7 octets each), control and PID. */
	int naddr = 0;
	for (int i = 0; i < f->len && i < 10 * 7; i += 7) {
	  naddr++;
	  if (f->data[i + 6] & 0x01) break;
	}
	int info_start = naddr * 7 + 2;
	std::string info;
	for (int i = info_start; i < f->len; i++) {
	  unsigned char c = f->data[i];
	  info += (c >= 0x20 && c < 0x7f) ? static_cast<char>(c) : '.';
	}

	const char *fec = f->fec == DW_EMBED_FEC_FX25 ? " FX.25" : f->fec == DW_EMBED_FEC_IL2P ? " IL2P" : "";
	std::printf("FRAME chan=%d sub=%d slice=%d level=%d%s len=%d  %s%s\n",
		    f->chan, f->subchan, f->slice, f->alevel_rec, fec, f->len, addrs, info.c_str());
	std::fflush(stdout);
	g_frames++;
}

void on_dcd(void *, int chan, int active)
{
	std::printf("DCD chan=%d %s\n", chan, active ? "busy" : "clear");
	std::fflush(stdout);
}

void on_ptt(void *, int chan, int active)
{
	std::printf("PTT chan=%d %s\n", chan, active ? "on" : "off");
	std::fflush(stdout);
}

void on_fault(void *, int status, const char *reason)
{
	std::printf("FAULT status=%d: %s\n", status, reason);
	std::fflush(stdout);
	g_quit = true;
}

/* Build a UI frame, PID 0xF0, from "SRC>DST,DIGI:text". */
bool encode_call(const std::string &text, unsigned char out[7], bool last)
{
	std::string call = text, ssid_s;
	auto dash = text.find('-');
	if (dash != std::string::npos) { call = text.substr(0, dash); ssid_s = text.substr(dash + 1); }
	if (call.empty() || call.size() > 6) return false;
	int ssid = ssid_s.empty() ? 0 : std::atoi(ssid_s.c_str());
	if (ssid < 0 || ssid > 15) return false;
	for (int i = 0; i < 6; i++) {
	  char c = i < static_cast<int>(call.size()) ? static_cast<char>(std::toupper(call[i])) : ' ';
	  out[i] = static_cast<unsigned char>(c << 1);
	}
	out[6] = static_cast<unsigned char>(0x60 | (ssid << 1) | (last ? 1 : 0));
	return true;
}

std::vector<unsigned char> build_ui_frame(const std::string &spec)
{
	std::vector<unsigned char> frame;
	auto gt = spec.find('>');
	auto colon = spec.find(':', gt == std::string::npos ? 0 : gt);
	if (gt == std::string::npos || colon == std::string::npos) return frame;

	std::string src = spec.substr(0, gt);
	std::string path = spec.substr(gt + 1, colon - gt - 1);
	std::string text = spec.substr(colon + 1);

	std::vector<std::string> addrs;
	size_t start = 0;
	while (true) {
	  auto comma = path.find(',', start);
	  addrs.push_back(path.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
	  if (comma == std::string::npos) break;
	  start = comma + 1;
	}
	if (addrs.empty() || addrs.size() > 9) return frame;

	unsigned char a[7];
	if (!encode_call(addrs[0], a, false)) return frame;			/* destination */
	frame.insert(frame.end(), a, a + 7);
	if (!encode_call(src, a, addrs.size() == 1)) return frame;		/* source */
	frame.insert(frame.end(), a, a + 7);
	for (size_t i = 1; i < addrs.size(); i++) {				/* digipeaters */
	  if (!encode_call(addrs[i], a, i + 1 == addrs.size())) return frame.clear(), frame;
	  frame.insert(frame.end(), a, a + 7);
	}
	frame.push_back(0x03);							/* UI */
	frame.push_back(0xF0);							/* no layer 3 */
	frame.insert(frame.end(), text.begin(), text.end());
	return frame;
}

void on_sigint(int) { g_quit = true; }

int run_once(const std::string &conf, int seconds, const std::string &send)
{
	dw_embed_callbacks_t cb{};
	cb.log = on_log;
	cb.frame_received = on_frame;
	cb.dcd_changed = on_dcd;
	cb.ptt_changed = on_ptt;
	cb.fault = on_fault;

	char err[600];
	if (dw_embed_start(conf.c_str(), &cb, err, sizeof(err)) != 0) {
	  std::printf("start failed: %s\n", err);
	  return 1;
	}

	char desc[100];
	dw_embed_channel_describe(0, desc, sizeof(desc));
	std::printf("channel 0: %s, MYCALL %s\n", desc, dw_embed_channel_mycall(0));

	std::thread engine([] { dw_embed_run(); });

	if (!send.empty()) {
	  auto frame = build_ui_frame(send);
	  if (frame.empty()) {
	    std::printf("could not parse frame spec \"%s\"\n", send.c_str());
	  }
	  else if (dw_embed_transmit(0, frame.data(), static_cast<int>(frame.size()), 0) != 0) {
	    std::printf("transmit refused\n");
	  }
	  else {
	    std::printf("queued %zu octets, TXBUF=%d\n", frame.size(), dw_embed_tx_queue_bytes(0));
	  }
	}

	auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
	int last_txbuf = -1;
	while (!g_quit && std::chrono::steady_clock::now() < deadline) {
	  std::this_thread::sleep_for(std::chrono::milliseconds(100));
	  int txbuf = dw_embed_tx_queue_bytes(0);
	  if (txbuf != last_txbuf) {
	    std::printf("TXBUF chan=0 %d bytes, DCD %d\n", txbuf, dw_embed_dcd(0));
	    last_txbuf = txbuf;
	  }
	}

	dw_embed_stop();
	engine.join();
	std::printf("run finished, %d frame(s) decoded\n", g_frames.load());
	return 0;
}

} // namespace

int main(int argc, char **argv)
{
	std::string conf, send;
	int seconds = 10;
	bool twice = false;

	for (int i = 1; i < argc; i++) {
	  std::string a = argv[i];
	  if (a == "-c" && i + 1 < argc) conf = argv[++i];
	  else if (a == "-t" && i + 1 < argc) seconds = std::atoi(argv[++i]);
	  else if (a == "-s" && i + 1 < argc) send = argv[++i];
	  else if (a == "--twice") twice = true;
	  else {
	    std::printf("usage: %s -c direwolf.conf [-t seconds] [-s \"SRC>DST,PATH:text\"] [--twice]\n", argv[0]);
	    return 2;
	  }
	}
	if (conf.empty()) {
	  std::printf("a configuration file is required (-c)\n");
	  return 2;
	}

	std::signal(SIGINT, on_sigint);
	std::printf("embedded Dire Wolf %s\n", dw_embed_direwolf_version());

	int rc = run_once(conf, seconds, send);
	if (rc == 0 && twice && !g_quit) {
	  std::printf("--- restarting the core ---\n");
	  rc = run_once(conf, seconds, send);
	}
	return rc;
}
