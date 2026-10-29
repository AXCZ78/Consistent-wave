#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <chrono>
#include <deque>

using namespace geode::prelude;

// Real time (not level time) so slow motion / time triggers / speedhack
// don't stretch the gap or leave a backlog of clicks.
static double nowSec() {
	return std::chrono::duration<double>(
		std::chrono::steady_clock::now().time_since_epoch()
	).count();
}

struct QueuedInput {
	bool down;
	int button;
	bool isPlayer1;
	double fireTime;
};

class $modify(EvenBaseLayer, GJBaseGameLayer) {
	struct Fields {
		std::deque<QueuedInput> queue;
		double lastFire = -1000.0;
		bool replaying = false;
	};

	void handleButton(bool down, int button, bool isPlayer1) {
		auto f = m_fields.self();

		if (f->replaying
			|| !Mod::get()->getSettingValue<bool>("enabled")
			|| !PlayLayer::get()) {
			return GJBaseGameLayer::handleButton(down, button, isPlayer1);
		}

		if (Mod::get()->getSettingValue<bool>("wave-only")) {
			auto player = isPlayer1 ? m_player1 : m_player2;
			if (!player || !player->m_isDart) {
				return GJBaseGameLayer::handleButton(down, button, isPlayer1);
			}
		}

		double gap = Mod::get()->getSettingValue<double>("gap-ms") / 1000.0;
		double maxDelay = Mod::get()->getSettingValue<double>("max-delay-ms") / 1000.0;
		double now = nowSec();

		double prev = f->queue.empty() ? f->lastFire : f->queue.back().fireTime;
		double fireAt = std::max(now, prev + gap);

		// Backlog too long: drop the stale queue and keep only the newest
		// input, so it always matches what you're actually doing now.
		if (fireAt - now > maxDelay) {
			f->queue.clear();
			fireAt = std::max(now, f->lastFire + gap);
		}

		if (f->queue.empty() && fireAt <= now) {
			f->lastFire = now;
			return GJBaseGameLayer::handleButton(down, button, isPlayer1);
		}

		f->queue.push_back({down, button, isPlayer1, fireAt});
	}

	void processCommands(float dt, bool isHalfTick, bool isLastTick) {
		auto f = m_fields.self();
		double now = nowSec();

		// At most one queued input per tick so a press and its release
		// never land in the same tick (matters in slow motion).
		if (!f->queue.empty() && f->queue.front().fireTime <= now) {
			auto ev = f->queue.front();
			f->queue.pop_front();
			f->lastFire = now;
			f->replaying = true;
			GJBaseGameLayer::handleButton(ev.down, ev.button, ev.isPlayer1);
			f->replaying = false;
		}

		GJBaseGameLayer::processCommands(dt, isHalfTick, isLastTick);
	}
};

class $modify(EvenPlayLayer, PlayLayer) {
	void resetLevel() {
		auto f = static_cast<EvenBaseLayer*>(static_cast<GJBaseGameLayer*>(this))->m_fields.self();
		f->queue.clear();
		f->lastFire = -1000.0;
		PlayLayer::resetLevel();
	}
};
