#include "Board.h"
#include "Sun.h"

void Board::InitializeCell(int rows, int cols)
{
	mRows = rows + 1;
	mColumns = cols + 1;
	mCells.resize(mRows);
    for (int i = 0; i < mRows; i++)
    {
        mCells[i].resize(mColumns);
        for (int j = 0; j < mColumns; j++)
        {
            Vector position(CELL_INITALIZE_POS_X + j * CELL_COLLIDER_SIZE_X,
                CELL_INITALIZE_POS_Y + i * CELL_COLLIDER_SIZE_Y);
            auto cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(i, j, position);
            mCells[i][j] = cell;
        }
    }
}

void Board::DrawCell(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // 半透明绿色
    for (auto& row : mCells) {
        for (auto& cell : row) {
            Vector pos = cell->GetWorldPosition();
            SDL_FRect rect = { pos.x, pos.y, CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y };
            SDL_RenderDrawRectF(renderer, &rect);
        }
    }
}

void Board::Draw(SDL_Renderer* renderer)
{
    DrawCell(renderer);
}

std::shared_ptr<Sun> Board::CreateSun(const Vector& position)
{
    auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>(this, position);

    mCoinObservers.push_back(sun);

    std::cout << "创建阳光，当前活跃Coin数量: " << GetActiveCoinCount()
        << "/" << GetTotalCreatedCoinCount() << std::endl;

    return sun;
}

void Board::CleanupExpiredObjects()
{
    // 清理所有已过期的引用
    mCoinObservers.erase(
        std::remove_if(mCoinObservers.begin(), mCoinObservers.end(),
            [](const std::weak_ptr<Coin>& weakCoin) {
                return weakCoin.expired();
            }),
        mCoinObservers.end()
    );
}

void Board::UpdateSunFalling()
{
    mSunCountDown -= DELTA_TIME;
    if (mSunCountDown <= 0.0f)
    {
        mSunCountDown = 5.0f;
        CreateSun(Vector(
            static_cast<float>(rand() % 700),
            static_cast<float>(rand() % 300)
		));
    }
}

void Board::Update()
{
    CleanupExpiredObjects();
}

int Board::GetActiveCoinCount() const
{
    // 统计当前活跃的 Coin 数量
    int count = 0;
    for (const auto& weakCoin : mCoinObservers) {
        if (!weakCoin.expired()) {
            count++;
        }
    }
    return count;
}