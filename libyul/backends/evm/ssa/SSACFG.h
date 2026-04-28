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
 * Control flow graph and stack layout structures used during code generation.
 */

#pragma once

#include <libyul/backends/evm/ssa/InstructionStore.h>
#include <libyul/backends/evm/ssa/SSACFGDebugInfo.h>
#include <libyul/backends/evm/ssa/SSACFGTypes.h>

#include <libyul/backends/evm/EVMDialect.h>

#include <libyul/AST.h>
#include <libyul/AsmAnalysisInfo.h>
#include <libyul/Dialect.h>
#include <libyul/Exceptions.h>

#include <libsolutil/Numeric.h>

#include <concepts>
#include <functional>
#include <string>
#include <vector>

namespace solidity::yul::ssa
{
class LivenessAnalysis;
struct ControlFlowGraphs;

class SSACFG
{
public:
	using DebugInfo = SSACFGDebugInfo;

	explicit SSACFG(
		EVMDialect const& _evmVersion,
		std::unique_ptr<DebugInfo> _debugInfo = nullptr
	):
		evmDialect(_evmVersion),
		debugInfo(std::move(_debugInfo))
	{}

	SSACFG(SSACFG const&) = delete;
	SSACFG(SSACFG&&) = delete;
	SSACFG& operator=(SSACFG const&) = delete;
	SSACFG& operator=(SSACFG&&) = delete;
	~SSACFG() = default;

	using BlockId = ssa::BlockId;
	using ValueId = ssa::ValueId;

	using BuiltinCall = InstructionStore::BuiltinCall;
	using Call = InstructionStore::Call;
	using Inst = InstructionStore::Inst;

	struct BasicBlock
	{
		struct MainExit {};
		struct ConditionalJump
		{
			ValueId condition;
			BlockId nonZero;
			BlockId zero;
		};
		struct Jump
		{
			BlockId target;
		};
		struct FunctionReturn
		{
			std::vector<ValueId> returnValues;
		};
		struct Terminated {};
		std::vector<BlockId> entries;
		std::vector<InstId> instructions;
		std::variant<MainExit, Jump, ConditionalJump, FunctionReturn, Terminated> exit = MainExit{};

		template<std::invocable<BlockId> Callable>
		void forEachExit(Callable&& _callable) const
		{
			if (auto* jump = std::get_if<Jump>(&exit))
				_callable(jump->target);
			else if (auto* conditionalJump = std::get_if<ConditionalJump>(&exit))
			{
				_callable(conditionalJump->nonZero);
				_callable(conditionalJump->zero);
			}
		}

		bool isMainExitBlock() const { return std::holds_alternative<MainExit>(exit); }
		bool isTerminationBlock() const { return std::holds_alternative<Terminated>(exit); }
		bool isFunctionReturnBlock() const { return std::holds_alternative<FunctionReturn>(exit); }
		bool isJumpBlock() const { return std::holds_alternative<Jump>(exit); }
	};

	BlockId makeBlock(langutil::DebugData::ConstPtr _debugData)
	{
		// max itself is reserved for the 'empty' block
		yulAssert(m_blocks.size() < std::numeric_limits<BlockId::ValueType>::max());
		BlockId const blockId{static_cast<BlockId::ValueType>(m_blocks.size())};
		m_blocks.emplace_back(BasicBlock{{}, {}, BasicBlock::Terminated{}});
		if (debugInfo)
			debugInfo->setBlockDebugData(blockId, std::move(_debugData));
		return blockId;
	}
	BasicBlock& block(BlockId _id) { return m_blocks.at(_id.value); }
	BasicBlock const& block(BlockId _id) const { return m_blocks.at(_id.value); }
	size_t numBlocks() const { return m_blocks.size(); }

	InstructionStore& instructionStore() { return m_instructions; }
	InstructionStore const& instructionStore() const { return m_instructions; }

	Inst& inst(InstId _id) { return m_instructions.inst(_id); }
	Inst const& inst(InstId _id) const { return m_instructions.inst(_id); }
	size_t numInsts() const { return m_instructions.numInsts(); }
	std::vector<Inst> const& instructions() const { return m_instructions.instructions(); }

	static auto outputsOf(InstId const _id, ValueId::OutputSize const _numOutputs)
	{
		return InstructionStore::outputsOf(_id, _numOutputs);
	}

	auto instOutputs(InstId const _id) const
	{
		return m_instructions.instOutputs(_id);
	}

	/// Returns the opcode category for a given ValueId.
	InstOpcode kindOf(ValueId const _v) const { return m_instructions.kindOf(_v); }

	/// Returns the phi targeted by an Upsilon Inst.
	ValueId upsilonPhi(InstId const _id) const { return m_instructions.upsilonPhi(_id); }

	/// Returns the u256 payload of a Const Inst.
	u256 const& literalPayload(InstId const _id) const { return m_instructions.literalPayload(_id); }

	BuiltinCall const& builtinPayload(InstId const _id) const { return m_instructions.builtinPayload(_id); }

	Call const& callPayload(InstId const _id) const { return m_instructions.callPayload(_id); }

	/// Creates a Phi Inst in the given block and returns its output ValueId.
	ValueId newPhi(BlockId const _definingBlock)
	{
		InstId const id = scheduleInBlock(m_instructions.appendPhi(_definingBlock), _definingBlock);
		ValueId const v{id};
		if (debugInfo)
			debugInfo->setValueDebugData(v, debugInfo->blockDebugData(_definingBlock));
		return v;
	}

	ValueId newFunctionArgument()
	{
		InstId const id = scheduleInBlock(m_instructions.appendFunctionArg(entry), entry);
		ValueId const v{id};
		if (debugInfo)
			debugInfo->setValueDebugData(v, debugInfo->blockDebugData(entry));
		return v;
	}

	ValueId unreachableValue()
	{
		return ValueId{m_instructions.appendUnreachable()};
	}

	/// Literal ValueIds are deduplicated. Const Insts are pinned to the entry block.
	ValueId newLiteral(langutil::DebugData::ConstPtr _debugData, u256 _value)
	{
		auto const beforeCount = m_instructions.numInsts();
		InstId const id = m_instructions.appendLiteral(entry, std::move(_value));
		ValueId const v{id};
		// Newly allocated (not deduplicated): schedule in entry and attach debug data.
		if (m_instructions.numInsts() > beforeCount)
		{
			scheduleInBlock(id, entry);
			if (debugInfo)
				debugInfo->setValueDebugData(v, std::move(_debugData));
		}
		return v;
	}

	InstId makeBuiltinCall(
		BlockId const _block,
		BuiltinCall _payload,
		std::vector<ValueId> _inputs,
		std::size_t const _numOutputs,
		langutil::DebugData::ConstPtr _debugData = {}
	)
	{
		InstId const id = scheduleInBlock(
			m_instructions.appendBuiltinCall(_block, std::move(_payload), std::move(_inputs), _numOutputs),
			_block
		);
		if (debugInfo && _debugData)
			debugInfo->setInstDebugData(id, std::move(_debugData));
		return id;
	}

	InstId makeCall(
		BlockId const _block,
		Call _payload,
		std::vector<ValueId> _inputs,
		std::size_t const _numOutputs,
		langutil::DebugData::ConstPtr _debugData = {}
	)
	{
		InstId const id = scheduleInBlock(
			m_instructions.appendCall(_block, std::move(_payload), std::move(_inputs), _numOutputs),
			_block
		);
		if (debugInfo && _debugData)
			debugInfo->setInstDebugData(id, std::move(_debugData));
		return id;
	}

	InstId emitUpsilon(BlockId const _block, ValueId _value, ValueId const _phi)
	{
		return scheduleInBlock(m_instructions.appendUpsilon(_block, _value, _phi), _block);
	}

	std::string toDot(
		bool _includeDiGraphDefinition=true,
		std::optional<size_t> _functionIndex=std::nullopt,
		LivenessAnalysis const* _liveness=nullptr,
		ControlFlowGraphs const* _controlFlow=nullptr
	) const;

private:
	InstId scheduleInBlock(InstId const _id, BlockId const _block)
	{
		m_blocks.at(_block.value).instructions.push_back(_id);
		return _id;
	}

	std::vector<BasicBlock> m_blocks;
	InstructionStore m_instructions;
public:
	EVMDialect const& evmDialect;
	std::unique_ptr<DebugInfo> debugInfo;
	BlockId entry = BlockId{0};
	std::set<BlockId> exits;
	std::string name{};
	bool canContinue = true;
	std::vector<ValueId> arguments;
	std::size_t numReturns = 0;

	bool isMainGraph() const { return name.empty(); }

	/// Iterates `_block.instructions`, invoking `_fn(instId, inst)` for every Phi.
	template<typename Callable>
	void forEachPhi(BasicBlock const& _block, Callable&& _fn)
	{
		forEachInstWhere(*this, _block, &Inst::isPhi, std::forward<Callable>(_fn));
	}
	template<typename Callable>
	void forEachPhi(BasicBlock const& _block, Callable&& _fn) const
	{
		forEachInstWhere(*this, _block, &Inst::isPhi, std::forward<Callable>(_fn));
	}

	/// Iterates `_block.instructions`, invoking `_fn(instId, inst)` for every Upsilon.
	template<typename Callable>
	void forEachUpsilon(BasicBlock const& _block, Callable&& _fn)
	{
		forEachInstWhere(*this, _block, &Inst::isUpsilon, std::forward<Callable>(_fn));
	}
	template<typename Callable>
	void forEachUpsilon(BasicBlock const& _block, Callable&& _fn) const
	{
		forEachInstWhere(*this, _block, &Inst::isUpsilon, std::forward<Callable>(_fn));
	}

	/// Iterates `_block.instructions`, invoking `_fn(instId, inst)` for every operation
	/// (Call or BuiltinCall).
	template<typename Callable>
	void forEachOperation(BasicBlock const& _block, Callable&& _fn)
	{
		forEachInstWhere(*this, _block, &Inst::isOperation, std::forward<Callable>(_fn));
	}
	template<typename Callable>
	void forEachOperation(BasicBlock const& _block, Callable&& _fn) const
	{
		forEachInstWhere(*this, _block, &Inst::isOperation, std::forward<Callable>(_fn));
	}

private:
	/// Shared implementation for the public `forEach*` overloads; works for both
	/// `SSACFG&` and `SSACFG const&` since `_self.inst(id)` propagates const-ness.
	template<typename Self, typename Pred, typename Callable>
	static void forEachInstWhere(Self& _self, BasicBlock const& _block, Pred _pred, Callable&& _fn)
	{
		for (InstId const id: _block.instructions)
		{
			auto& inst = _self.inst(id);
			if (std::invoke(_pred, inst))
				_fn(id, inst);
		}
	}
};

}
