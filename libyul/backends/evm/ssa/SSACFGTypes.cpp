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

#include <libyul/backends/evm/ssa/SSACFGTypes.h>

#include <libyul/backends/evm/ssa/SSACFG.h>

#include <libsolutil/CommonData.h>
#include <libsolutil/Numeric.h>

using namespace solidity::util;
using namespace solidity::yul::ssa;

std::string ValueId::str(SSACFG const& _cfg) const
{
	if (!hasValue())
		return "INVALID";
	switch (kind())
	{
		case Kind::Literal:  return toCompactHexWithPrefix(_cfg.literalInfo(*this).value);
		case Kind::Variable: return fmt::format("v{}", value());
		case Kind::Phi: return fmt::format("phi{}", value());
		case Kind::Unreachable: return "[unreachable]";
	}
	unreachable();
}
