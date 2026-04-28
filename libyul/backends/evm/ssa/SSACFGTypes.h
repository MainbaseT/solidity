/*
	This file is part of solidity.

	solidity is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	solidity is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
// SPDX-License-Identifier: GPL-3.0
/**
 * Types used throughout the SSA CFG.
 */

#pragma once

#include <fmt/format.h>

#include <cstdint>
#include <limits>
#include <string>

namespace solidity::yul::ssa
{

class SSACFG;

using FunctionGraphID = std::uint32_t;

struct BlockId
{
	using ValueType = std::uint32_t;
	ValueType value = std::numeric_limits<ValueType>::max();
	constexpr bool hasValue() const noexcept { return value != std::numeric_limits<ValueType>::max(); }
	auto operator<=>(BlockId const&) const = default;
};

struct InstId
{
	using ValueType = std::uint32_t;
	ValueType value = std::numeric_limits<ValueType>::max();
	constexpr bool hasValue() const noexcept { return value != std::numeric_limits<ValueType>::max(); }
	auto operator<=>(InstId const&) const = default;
};

enum class InstOpcode : std::uint8_t
{
	Const,  // literal u256
	Phi,  // no inputs, retrieves value by mapping
	Upsilon,  // records phi pre-image at a phi predecessor block
	BuiltinCall,  // dialect builtin
	Call,  // user-defined function call
	Unreachable,  // dead code
	FunctionArg, // function param without inputs and a single output valueid
};

/// Identifies a specific produced value of an Inst.
class ValueId
{
public:
	// this value refers to the n-th return value of an operation
	using OutputSize = std::uint8_t;

	constexpr ValueId() = default;
	constexpr ValueId(InstId const _instId, OutputSize const _outputPos = 0):
		m_instId(_instId),
		m_outputPos(_outputPos)
	{}

	bool constexpr hasValue() const noexcept { return m_instId.hasValue(); }
	OutputSize constexpr outputPos() const noexcept { return m_outputPos; }
	InstId constexpr instId() const noexcept { return m_instId; }

	/// Returns a human-readable string representation
	std::string str(SSACFG const& _cfg) const;

	auto operator<=>(ValueId const&) const = default;

private:
	InstId m_instId{};
	OutputSize m_outputPos{0};
};

}

template<>
struct fmt::formatter<solidity::yul::ssa::BlockId>
{
	static auto constexpr parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

	template<typename FormatContext>
	auto format(solidity::yul::ssa::BlockId const& _blockId, FormatContext& _ctx) const -> decltype(_ctx.out())
	{
		if (!_blockId.hasValue())
			return fmt::format_to(_ctx.out(), "empty");
		return fmt::format_to(_ctx.out(), "{}", _blockId.value);
	}
};

template<>
struct fmt::formatter<solidity::yul::ssa::InstId>
{
	static auto constexpr parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

	template<typename FormatContext>
	auto format(solidity::yul::ssa::InstId const& _instId, FormatContext& _ctx) const -> decltype(_ctx.out())
	{
		if (!_instId.hasValue())
			return fmt::format_to(_ctx.out(), "empty");
		return fmt::format_to(_ctx.out(), "i{}", _instId.value);
	}
};

template<>
struct fmt::formatter<solidity::yul::ssa::ValueId>
{
	static auto constexpr parse(format_parse_context& ctx) -> decltype(ctx.begin()) { return ctx.begin(); }

	template<typename FormatContext>
	auto format(solidity::yul::ssa::ValueId const& _valueId, FormatContext& _ctx) const -> decltype(_ctx.out())
	{
		if (!_valueId.hasValue())
			return fmt::format_to(_ctx.out(), "empty");
		if (_valueId.outputPos() == 0)
			return fmt::format_to(_ctx.out(), "v{}", _valueId.instId().value);
		return fmt::format_to(_ctx.out(), "v{}.{}", _valueId.instId().value, _valueId.outputPos());
	}
};
