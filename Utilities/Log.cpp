﻿#include "Log.h"

namespace _log
{
	static file_listener& get_logger()
	{
		// Use magic static
		static file_listener logger("RPCS3.log");
		return logger;
	}

	file_writer g_tty_file("TTY.log");

	channel GENERAL(nullptr, level::notice);
	channel LOADER("LDR", level::notice);
	channel MEMORY("MEM", level::notice);
	channel RSX("RSX", level::notice);
	channel HLE("HLE", level::notice);
	channel PPU("PPU", level::notice);
	channel SPU("SPU", level::notice);
	channel ARMv7("ARMv7");

	thread_local std::string(*g_tls_make_prefix)(const channel&, level, const std::string&) = nullptr;
}

void _log::broadcast(const _log::channel& ch, _log::level sev, const std::string& text)
{
	get_logger().log(ch, sev, text);
}

[[noreturn]] extern void catch_all_exceptions();

_log::file_writer::file_writer(const std::string& name)
{
	try
	{
		if (!m_file.open(fs::get_config_dir() + name, fs::rewrite + fs::append))
		{
			throw fmt::exception("Can't create log file %s (error %d)", name, errno);
		}
	}
	catch (...)
	{
		catch_all_exceptions();
	}
}

void _log::file_writer::log(const std::string& text)
{
	m_file.write(text);
}

std::size_t _log::file_writer::size() const
{
	return m_file.pos();
}

void _log::file_listener::log(const _log::channel& ch, _log::level sev, const std::string& text)
{
	std::string msg; msg.reserve(text.size() + 200);

	// Used character: U+00B7 (Middle Dot)
	switch (sev)
	{
	case level::always:  msg = u8"·A "; break;
	case level::fatal:   msg = u8"·F "; break;
	case level::error:   msg = u8"·E "; break;
	case level::todo:    msg = u8"·U "; break;
	case level::success: msg = u8"·S "; break;
	case level::warning: msg = u8"·W "; break;
	case level::notice:  msg = u8"·! "; break;
	case level::trace:   msg = u8"·T "; break;
	}

	// TODO: print time?

	if (auto func = g_tls_make_prefix)
	{
		msg += '{';
		msg += func(ch, sev, text);
		msg += "} ";
	}
	
	if (ch.name)
	{
		msg += ch.name;
		msg += sev == level::todo ? " TODO: " : ": ";
	}
	else if (sev == level::todo)
	{
		msg += "TODO: ";
	}
	
	msg += text;
	msg += '\n';

	file_writer::log(msg);
}
