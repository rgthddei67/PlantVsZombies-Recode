#include "Board.h"
#include "Game/Zombie/ExcavatorZombie.h"

Zombie* Board::GetMineWallOwner(int cell) const
{
	if (cell < 0 || cell >= MineGrid::Count) return nullptr;
	auto* owner = dynamic_cast<ExcavatorZombie*>(mEntityRegistry.GetZombie(mMineWallOwners[cell]));
	return owner && owner->IsActive() && !owner->IsDying() && owner->HasHead()
		&& !owner->IsMindControlled() && owner->GetWall() == cell ? owner : nullptr;
}

bool Board::ReserveMineExcavation(Zombie* zombie, int start, int& wall, int& stand)
{
	if (!zombie || !IsMineBackground()) return false;
	std::array<bool,MineGrid::Count> excluded{};
	for (int cell = 0; cell < MineGrid::Count; ++cell)
		excluded[cell] = cell == mMineDigCell || GetMineWallOwner(cell) != nullptr;
	if (!mMineGrid.FindExcavation(start,excluded,wall,stand)) return false;
	mMineWallOwners[wall] = zombie->mZombieID;
	return true;
}
