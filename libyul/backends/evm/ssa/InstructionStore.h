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
 * Owns the instruction table of an SSACFG: hands out InstIds, stores opcode
 * payloads, and provides lookups. Has no knowledge of basic blocks or debug
 * information; SSACFG composes those on top.
 */

#pragma once

#include <libyul/backends/evm/ssa/SSACFGTypes.h>

#include <libyul/AST.h>
#include <libyul/Builtins.h>
#include <libyul/Exceptions.h>

#include <libsolutil/Numeric.h>

#include <limits>
#include <map>
#include <optional>
#include <variant>
#include <vector>

namespace solidity::yul::ssa
{

class InstructionStore
{
public:
	struct BuiltinCall
	{
		BuiltinHandle builtin;
		/// Literal-kind arguments
		std::vector<Literal> literalArguments;
	};
	struct Call
	{
		FunctionGraphID graphID;
		bool canContinue;
	};
	struct Operation
	{
		std::vector<ValueId> outputs{};
		std::variant<BuiltinCall, Call> kind;
		BlockId block{};
		std::vector<ValueId> inputs{};
	};
	struct LiteralValue
	{
		u256 value;
	};
	struct VariableValue
	{
		BlockId definingBlock;
	};
	struct PhiValue
	{
		BlockId block;
	};
	struct UnreachableValue {};

	InstructionStore() = default;
	InstructionStore(InstructionStore const&) = delete;
	InstructionStore(InstructionStore&&) = default;
	InstructionStore& operator=(InstructionStore const&) = delete;
	InstructionStore& operator=(InstructionStore&&) = default;
	~InstructionStore() = default;

	Operation& operation(InstId const _id) { return m_operations.at(_id.value); }
	Operation const& operation(InstId const _id) const { return m_operations.at(_id.value); }

	LiteralValue const& literalInfo(ValueId const& _valueId) const
	{
		yulAssert(_valueId.hasValue() && _valueId.isLiteral());
		return m_literals.at(_valueId.value());
	}

	PhiValue const& phiInfo(ValueId const& _valueId) const
	{
		yulAssert(_valueId.hasValue() && _valueId.isPhi());
		return m_phis.at(_valueId.value());
	}
	PhiValue& phiInfo(ValueId const& _valueId)
	{
		yulAssert(_valueId.hasValue() && _valueId.isPhi());
		return m_phis.at(_valueId.value());
	}

	VariableValue const& variableInfo(ValueId const& _valueId) const
	{
		yulAssert(_valueId.hasValue() && _valueId.isVariable());
		return m_variables.at(_valueId.value());
	}

	ValueId newPhi(BlockId const _definingBlock)
	{
		yulAssert(_definingBlock.hasValue());
		m_phis.emplace_back(PhiValue{_definingBlock});
		auto const value = m_phis.size() - 1;
		yulAssert(value < std::numeric_limits<ValueId::ValueType>::max());
		return ValueId::makePhi(static_cast<ValueId::ValueType>(value));
	}

	ValueId newVariable(BlockId const _definingBlock)
	{
		yulAssert(_definingBlock.hasValue());
		m_variables.emplace_back(VariableValue{_definingBlock});
		auto const value = m_variables.size() - 1;
		yulAssert(value < std::numeric_limits<ValueId::ValueType>::max());
		return ValueId::makeVariable(static_cast<ValueId::ValueType>(value));
	}

	ValueId unreachableValue()
	{
		if (!m_unreachableValue)
			m_unreachableValue = ValueId::makeUnreachable();
		return *m_unreachableValue;
	}

	/// Look up or allocate a Literal ValueId for `_value`. Returns the ValueId and
	/// whether a new entry was allocated (false if it was deduplicated).
	std::pair<ValueId, bool> newLiteral(u256 _value)
	{
		if (auto const it = m_literalMapping.find(_value); it != m_literalMapping.end())
		{
			yulAssert(it->second.hasValue() && m_literals[it->second.value()].value == _value);
			return {it->second, false};
		}
		m_literals.emplace_back(LiteralValue{std::move(_value)});
		auto const idx = m_literals.size() - 1;
		yulAssert(idx < std::numeric_limits<ValueId::ValueType>::max());
		auto const id = ValueId::makeLiteral(static_cast<ValueId::ValueType>(idx));
		m_literalMapping.emplace(m_literals.back().value, id);
		return {id, true};
	}

	InstId makeOperation(BlockId const _block, Operation _op)
	{
		yulAssert(_block.hasValue());
		for (ValueId const& output: _op.outputs)
		{
			yulAssert(output.isVariable());
			yulAssert(m_variables.at(output.value()).definingBlock == _block);
		}
		_op.block = _block;
		InstId const id{static_cast<InstId::ValueType>(m_operations.size())};
		m_operations.emplace_back(std::move(_op));
		return id;
	}

private:
	std::vector<Operation> m_operations;
	std::vector<LiteralValue> m_literals;
	std::map<u256, ValueId> m_literalMapping;
	std::vector<PhiValue> m_phis;
	std::vector<VariableValue> m_variables;
	std::optional<ValueId> m_unreachableValue;
};

}
