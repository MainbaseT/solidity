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

#include <libyul/backends/evm/ssa/transform/UnreachableBlockCleaner.h>

#include <libyul/backends/evm/ssa/SSACFG.h>

#include <libsolutil/Algorithms.h>
#include <libsolutil/Visitor.h>

using namespace solidity;
using namespace solidity::yul;
using namespace solidity::yul::ssa;
using namespace solidity::yul::ssa::transform;

void transform::cleanUnreachableBlocks(SSACFG& _cfg)
{
	util::BreadthFirstSearch<SSACFG::BlockId> reachabilityCheck{{_cfg.entry}};
	reachabilityCheck.run([&](SSACFG::BlockId const _blockId, auto&& _addChild) {
		auto const& block = _cfg.block(_blockId);
		visit(util::GenericVisitor{
			[&](SSACFG::BasicBlock::Jump const& _jump) { _addChild(_jump.target); },
			[&](SSACFG::BasicBlock::ConditionalJump const& _jump) {
				_addChild(_jump.zero);
				_addChild(_jump.nonZero);
			},
			[](SSACFG::BasicBlock::FunctionReturn const&) {},
			[](SSACFG::BasicBlock::Terminated const&) {},
			[](SSACFG::BasicBlock::MainExit const&) {}
		}, block.exit);
	});

	for (SSACFG::BlockId const blockId: reachabilityCheck.visited)
	{
		auto& block = _cfg.block(blockId);
		std::erase_if(block.entries, [&](auto const& entry) {
			return !reachabilityCheck.visited.contains(entry);
		});
	}

	for (SSACFG::BlockId blockId{0}; blockId.value < _cfg.numBlocks(); ++blockId.value)
		if (!reachabilityCheck.visited.contains(blockId))
			_cfg.block(blockId).upsilons.clear();
}
