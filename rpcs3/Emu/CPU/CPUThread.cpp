#include "stdafx.h"
#include "Emu/System.h"
#include "Emu/IdManager.h"

#include "Emu/Cell/PPUThread.h"
#include "Emu/Cell/SPUThread.h"
#include "Emu/Cell/RawSPUThread.h"
#include "Emu/ARMv7/ARMv7Thread.h"
#include "CPUThread.h"

thread_local cpu_thread* g_tls_current_cpu_thread = nullptr;

void cpu_thread::on_task()
{
	g_tls_current_cpu_thread = this;

	Emu.SendDbgCommand(DID_CREATE_THREAD, this);

	std::unique_lock<std::mutex> lock(mutex);

	// Check thread status
	while (!(state & cpu_state::exit))
	{
		CHECK_EMU_STATUS;

		// check stop status
		if (!(state & cpu_state::stop))
		{
			if (lock) lock.unlock();

			try
			{
				cpu_task();
			}
			catch (cpu_state _s)
			{
				state += _s;
			}
			catch (const std::exception&)
			{
				LOG_NOTICE(GENERAL, "\n%s", dump());
				throw;
			}

			state -= cpu_state::ret;
			continue;
		}

		if (!lock)
		{
			lock.lock();
			continue;
		}

		cv.wait(lock);
	}
}

bool cpu_thread::check_status()
{
	std::unique_lock<std::mutex> lock(mutex, std::defer_lock);

	while (true)
	{
		CHECK_EMU_STATUS; // check at least once

		if (state & cpu_state::exit)
		{
			return true;
		}

		if (!state.test(cpu_state_pause) && !state.test(cpu_state::interrupt))
		{
			break;
		}

		if (!lock)
		{
			lock.lock();
			continue;
		}

		if (!state.test(cpu_state_pause) && state & cpu_state::interrupt && handle_interrupt())
		{
			continue;
		}

		cv.wait(lock);
	}

	const auto state_ = state.load();

	if (state_ & to_mset(cpu_state::ret, cpu_state::stop))
	{
		return true;
	}

	if (state_ & cpu_state::dbg_step)
	{
		state += cpu_state::dbg_pause;
		state -= cpu_state::dbg_step;
	}

	return false;
}

std::vector<std::shared_ptr<cpu_thread>> get_all_cpu_threads()
{
	std::vector<std::shared_ptr<cpu_thread>> result;

	for (auto& t : idm::get_all<PPUThread>())
	{
		result.emplace_back(t);
	}

	for (auto& t : idm::get_all<SPUThread>())
	{
		result.emplace_back(t);
	}

	for (auto& t : idm::get_all<RawSPUThread>())
	{
		result.emplace_back(t);
	}

	for (auto& t : idm::get_all<ARMv7Thread>())
	{
		result.emplace_back(t);
	}

	return result;
}
