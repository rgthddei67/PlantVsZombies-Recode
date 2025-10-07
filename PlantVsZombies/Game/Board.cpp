#include "Board.h"

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
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // °ëÍ¸Ã÷ÂÌÉ«
    for (auto& row : mCells) {
        for (auto& cell : row) {
            Vector pos = cell->GetWorldPosition();
            SDL_FRect rect = { pos.x, pos.y, CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y };
            SDL_RenderDrawRectF(renderer, &rect);
        }
    }
}