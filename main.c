#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 1.0f
#define BOARD_SIZE 100

#define GRAVITY -35.0f
#define JUMP_FORCE 10.0f
#define RESPAWN_Y_THRESHOLD -10.0f

#define CAMERA_HEIGHT 2.0f
#define CAMERA_SPEED 4.0f
#define MOUSE_SENSITIVITY 0.003f
#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 2.0f
#define SPAWN_POSITION (Vector3){ 50.0f, CAMERA_HEIGHT, 50.0f }

#define GHOST_BLOCK_MIN_ALPHA 0.3f
#define GHOST_BLOCK_MAX_ALPHA 0.7f
#define GHOST_BLOCK_SPEED 2.0f
#define CUBE_WIRE_OFFSET 1.001f

#define GROUND_THICKNESS 0.1f

#define CROSSHAIR_SIZE 10
#define FPS_TEXT_SIZE 20
#define FPS_UPDATE_INTERVAL 0.1f

#define MAX_FADING_BLOCKS 100
#define FADE_TIME 0.1f

typedef enum {
    BLOCK_STONE = 1,
    BLOCK_GRASS = 2,
    BLOCK_DIRT = 3,
    BLOCK_WOOD = 4
} BlockType;

typedef struct {
    Vector3 position;
    bool active;
    BlockType type;
} Block;

typedef struct {
    Vector3 position;
    float fadeTimer;
    BlockType type;
    bool active;
} FadingBlock;

Block* blocks = NULL;
int blockCount = 0;
int blockCapacity = 0;

FadingBlock* fadingBlocks = NULL;
int fadingBlockCount = 0;
int fadingBlockCapacity = 0;

BlockType selectedBlockType = BLOCK_STONE;

bool CheckPlayerCollision(Vector3 playerPos, Block* blocks, int blockCount);
float FindGroundLevel(Vector3 position, Block* blocks, int blockCount);
bool IsPlayerSupported(Vector3 playerPos, Block* blocks, int blockCount);
void _gc();

void DrawBlockPreview(Texture2D texture) {
    int previewSize = 64;
    int padding = 20;
    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    Rectangle destRec = { screenWidth - previewSize - padding, padding, previewSize, previewSize };
    DrawTexturePro(texture, (Rectangle){0, 0, texture.width, texture.height}, destRec, (Vector2){0,0}, 0.0f, WHITE);
}

bool CheckAABBCollision(BoundingBox box1, BoundingBox box2) {
    return (box1.min.x <= box2.max.x && box1.max.x >= box2.min.x) &&
           (box1.min.y <= box2.max.y && box1.max.y >= box2.min.y) &&
           (box1.min.z <= box2.max.z && box1.max.z >= box2.min.z);
}

bool CheckPlayerCollision(Vector3 playerPos, Block* blocks, int blockCount) {
    BoundingBox playerBox = {
        (Vector3){ playerPos.x - PLAYER_RADIUS, playerPos.y - PLAYER_HEIGHT / 2.0f, playerPos.z - PLAYER_RADIUS },
        (Vector3){ playerPos.x + PLAYER_RADIUS, playerPos.y + PLAYER_HEIGHT / 2.0f, playerPos.z + PLAYER_RADIUS }
    };

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            BoundingBox blockBox = {
                (Vector3){ blocks[i].position.x - 0.5f, blocks[i].position.y - 0.5f, blocks[i].position.z - 0.5f },
                (Vector3){ blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f }
            };

            if (CheckAABBCollision(playerBox, blockBox)) {
                return true;
            }
        }
    }
    return false;
}

Vector3 CheckMoveCollision(Vector3 newPos, Block* blocks, int blockCount) {
    BoundingBox playerBox = {
        (Vector3){ newPos.x - PLAYER_RADIUS, newPos.y - PLAYER_HEIGHT/2.0f, newPos.z - PLAYER_RADIUS },
        (Vector3){ newPos.x + PLAYER_RADIUS, newPos.y + PLAYER_HEIGHT/2.0f, newPos.z + PLAYER_RADIUS }
    };

    Vector3 correction = {0.0f, 0.0f, 0.0f};

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            BoundingBox blockBox = {
                (Vector3){ blocks[i].position.x - 0.5f, blocks[i].position.y - 0.5f, blocks[i].position.z - 0.5f },
                (Vector3){ blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f }
            };

            if (CheckAABBCollision(playerBox, blockBox)) {

                float overlapX = fminf(playerBox.max.x - blockBox.min.x, blockBox.max.x - playerBox.min.x);
                float overlapY = fminf(playerBox.max.y - blockBox.min.y, blockBox.max.y - playerBox.min.y);
                float overlapZ = fminf(playerBox.max.z - blockBox.min.z, blockBox.max.z - playerBox.min.z);

                if (overlapX < overlapY && overlapX < overlapZ) {
                    if (playerBox.max.x - blockBox.min.x < blockBox.max.x - playerBox.min.x) {
                        correction.x = -overlapX;
                    } else {
                        correction.x = overlapX;
                    }
                } else if (overlapY < overlapX && overlapY < overlapZ) {
                    if (playerBox.max.y - blockBox.min.y < blockBox.max.y - playerBox.min.y) {
                        correction.y = -overlapY;
                    } else {
                        correction.y = overlapY;
                    }
                } else {
                    if (playerBox.max.z - blockBox.min.z < blockBox.max.z - playerBox.min.z) {
                        correction.z = -overlapZ;
                    } else {
                        correction.z = overlapZ;
                    }
                }

                return correction;
            }
        }
    }
    return correction;
}

float FindGroundLevel(Vector3 position, Block* blocks, int blockCount) {
    float groundLevel = 0.0f;
    int targetX = (int)roundf(position.x);
    int targetZ = (int)roundf(position.z);

    if (targetX >= 0 && targetX < BOARD_SIZE && targetZ >= 0 && targetZ < BOARD_SIZE) {
        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                if (roundf(blocks[i].position.x) == targetX && roundf(blocks[i].position.z) == targetZ) {
                    float blockTop = blocks[i].position.y + 0.5f;
                    if (blockTop > groundLevel) {
                        groundLevel = blockTop;
                    }
                }
            }
        }
        return groundLevel;
    } else {
        return -1000.0f;
    }
}

bool IsPlayerSupported(Vector3 playerPos, Block* blocks, int blockCount) {
    BoundingBox playerFeetBox = {
        (Vector3){ playerPos.x - PLAYER_RADIUS, playerPos.y - PLAYER_HEIGHT / 2.0f - 0.1f, playerPos.z - PLAYER_RADIUS },
        (Vector3){ playerPos.x + PLAYER_RADIUS, playerPos.y - PLAYER_HEIGHT / 2.0f, playerPos.z + PLAYER_RADIUS }
    };

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            BoundingBox blockTopBox = {
                (Vector3){ blocks[i].position.x - 0.5f, blocks[i].position.y + 0.5f - 0.1f, blocks[i].position.z - 0.5f },
                (Vector3){ blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f }
            };

            if (CheckAABBCollision(playerFeetBox, blockTopBox)) {
                return true;
            }
        }
    }

    if (playerFeetBox.min.y <= 0.5f && playerFeetBox.max.y >= -0.5f) {
        if (playerPos.x >= 0.0f && playerPos.x <= BOARD_SIZE && playerPos.z >= 0.0f && playerPos.z <= BOARD_SIZE) {
            return true;
        }
    }

    return false;
}

void _gc() {
    int newBlockCount = 0;
    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            blocks[newBlockCount] = blocks[i];
            newBlockCount++;
        }
    }
    blockCount = newBlockCount;

    int newFadingBlockCount = 0;
    for (int i = 0; i < fadingBlockCount; i++) {
        if (fadingBlocks[i].active) {
            fadingBlocks[newFadingBlockCount] = fadingBlocks[i];
            newFadingBlockCount++;
        }
    }
    fadingBlockCount = newFadingBlockCount;
}

int main(void) {
    SetTraceLogLevel(LOG_ERROR);
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "box");

    int screenWidth = GetScreenWidth();
    int screenHeight = GetScreenHeight();

    bool mouseCaptured = true;
    bool skipNextClick = false;
    DisableCursor();

    Camera camera = { 0 };
    camera.position = SPAWN_POSITION;
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 100.0f;
    camera.projection = CAMERA_PERSPECTIVE;

    Vector3 forward = (Vector3){ 0.0f, 0.0f, 1.0f };
    float velocityY = 0.0f;
    bool isGrounded = true;
    float yaw = 0.0f;
    float pitch = 0.0f;

    int displayedFPS = 60;
    float fpsUpdateTimer = 0.0f;

    blockCapacity = 100;
    blocks = (Block*)malloc(blockCapacity * sizeof(Block));

    fadingBlockCapacity = MAX_FADING_BLOCKS;
    fadingBlocks = (FadingBlock*)malloc(fadingBlockCapacity * sizeof(FadingBlock));

    Texture2D stoneTexture = LoadTexture("stone.png");
    Texture2D grassTexture = LoadTexture("grass.png");
    Texture2D dirtTexture = LoadTexture("dirt.png");
    Texture2D woodTexture = LoadTexture("wood.png");

    SetTextureFilter(grassTexture, TEXTURE_FILTER_POINT);
    SetTextureFilter(stoneTexture, TEXTURE_FILTER_POINT);
    SetTextureFilter(dirtTexture, TEXTURE_FILTER_POINT);
    SetTextureFilter(woodTexture, TEXTURE_FILTER_POINT);

    Material stoneMaterial = LoadMaterialDefault();
    stoneMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = stoneTexture;

    Material grassMaterial = LoadMaterialDefault();
    grassMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;

    Material dirtMaterial = LoadMaterialDefault();
    dirtMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = dirtTexture;

    Material woodMaterial = LoadMaterialDefault();
    woodMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = woodTexture;

    Material* blockMaterials[5];
    blockMaterials[BLOCK_STONE] = &stoneMaterial;
    blockMaterials[BLOCK_GRASS] = &grassMaterial;
    blockMaterials[BLOCK_DIRT] = &dirtMaterial;
    blockMaterials[BLOCK_WOOD] = &woodMaterial;

    Texture2D blockTextures[5];
    blockTextures[BLOCK_STONE] = stoneTexture;
    blockTextures[BLOCK_GRASS] = grassTexture;
    blockTextures[BLOCK_DIRT] = dirtTexture;
    blockTextures[BLOCK_WOOD] = woodTexture;

    Model blockModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Model ghostBlockModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Model groundModel = LoadModelFromMesh(GenMeshPlane((float)BOARD_SIZE, (float)BOARD_SIZE, BOARD_SIZE, BOARD_SIZE));

    Material groundMaterial = LoadMaterialDefault();
    groundMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;

    groundModel.materials[0] = groundMaterial;

    float* texcoords = groundModel.meshes[0].texcoords;
    int vertices_x = BOARD_SIZE + 1;
    int vertices_y = BOARD_SIZE + 1;

    for (int i = 0; i < vertices_x; i++) {
        for (int j = 0; j < vertices_y; j++) {
            texcoords[2 * (i + j * vertices_x)] = (float)i;
            texcoords[2 * (i + j * vertices_x) + 1] = (float)j;
        }
    }

    UpdateMeshBuffer(groundModel.meshes[0], 1, groundModel.meshes[0].texcoords, groundModel.meshes[0].vertexCount * 2 * sizeof(float), 0);

    while (!WindowShouldClose()) {
        float deltaTime = GetFrameTime();

        fpsUpdateTimer += deltaTime;
        if (fpsUpdateTimer >= FPS_UPDATE_INTERVAL) {
            displayedFPS = GetFPS();
            fpsUpdateTimer = 0.0f;
        }

        if (IsWindowResized()) {
            screenWidth = GetScreenWidth();
            screenHeight = GetScreenHeight();
        }

        if (IsKeyPressed(KEY_ONE)) selectedBlockType = BLOCK_STONE;
        if (IsKeyPressed(KEY_TWO)) selectedBlockType = BLOCK_GRASS;
        if (IsKeyPressed(KEY_THREE)) selectedBlockType = BLOCK_DIRT;
        if (IsKeyPressed(KEY_FOUR)) selectedBlockType = BLOCK_WOOD;

        if (IsKeyPressed(KEY_BACKSPACE)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) {
                DisableCursor();
                SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
                skipNextClick = true;
            } else {
                EnableCursor();
                skipNextClick = false;
            }
        }

        if (!mouseCaptured && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            if (GetMousePosition().x >= 0 && GetMousePosition().x < GetScreenWidth() &&
                GetMousePosition().y >= 0 && GetMousePosition().y < GetScreenHeight()) {
                mouseCaptured = true;
                DisableCursor();
                SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
                skipNextClick = true;

                goto skip_mouse_input;
            }
        }

        if (skipNextClick) {
            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) || IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
                skipNextClick = false;
                goto skip_mouse_input;
            }
        }

        if (mouseCaptured) {
            Vector2 mouseDelta = GetMouseDelta();
            yaw += mouseDelta.x * MOUSE_SENSITIVITY;
            pitch -= mouseDelta.y * MOUSE_SENSITIVITY;

            const float maxPitch = 89.0f * DEG2RAD;
            if (pitch > maxPitch) pitch = maxPitch;
            if (pitch < -maxPitch) pitch = -maxPitch;

            forward.x = cosf(pitch) * cosf(yaw);
            forward.y = sinf(pitch);
            forward.z = cosf(pitch) * sinf(yaw);

            forward = Vector3Normalize(forward);
            camera.target = Vector3Add(camera.position, forward);
        }

        Vector3 move = { 0 };
        Vector3 forwardDir = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        forwardDir.y = 0;
        forwardDir = Vector3Normalize(forwardDir);
        Vector3 rightDir = Vector3Normalize(Vector3CrossProduct(forwardDir, camera.up));

        if (IsKeyDown(KEY_W)) move = Vector3Add(move, forwardDir);
        if (IsKeyDown(KEY_S)) move = Vector3Subtract(move, forwardDir);
        if (IsKeyDown(KEY_D)) move = Vector3Add(move, rightDir);
        if (IsKeyDown(KEY_A)) move = Vector3Subtract(move, rightDir);

        if (Vector3Length(move) > 0) {
            move = Vector3Normalize(move);
        }

        Vector3 newPos = Vector3Add(camera.position, Vector3Scale(move, CAMERA_SPEED * deltaTime));
        Vector3 collisionCorrection = CheckMoveCollision(newPos, blocks, blockCount);
        if (collisionCorrection.x != 0 || collisionCorrection.z != 0) {
            newPos = Vector3Add(newPos, (Vector3){collisionCorrection.x, 0.0f, collisionCorrection.z});
        }
        camera.position = newPos;

        isGrounded = IsPlayerSupported(camera.position, blocks, blockCount);
        if (isGrounded && IsKeyDown(KEY_SPACE)) {
            velocityY = JUMP_FORCE;
        } else if (!isGrounded) {
            velocityY += GRAVITY * deltaTime;
        } else {
            velocityY = 0.0f;
        }

        newPos = camera.position;
        newPos.y += velocityY * deltaTime;
        collisionCorrection = CheckMoveCollision(newPos, blocks, blockCount);
        if (collisionCorrection.y != 0.0f) {
            newPos.y += collisionCorrection.y;
            velocityY = 0.0f;
        }
        camera.position = newPos;

        if (camera.position.y < RESPAWN_Y_THRESHOLD) {
            camera.position = SPAWN_POSITION;
            velocityY = 0.0f;
        }

        camera.target = Vector3Add(camera.position, forward);

        Ray ray = GetMouseRay((Vector2){screenWidth / 2.0f, screenHeight / 2.0f}, camera);

        Vector3 ghostBlockPos = {0};
        bool showGhostBlock = false;
        int hitBlockIndex = -1;
        float shortestDistance = 10000.0f;

        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                BoundingBox blockBox = {
                    Vector3SubtractValue(blocks[i].position, 0.5f),
                    Vector3AddValue(blocks[i].position, 0.5f)
                };
                RayCollision hitInfo = GetRayCollisionBox(ray, blockBox);
                if (hitInfo.hit && hitInfo.distance < shortestDistance) {
                    shortestDistance = hitInfo.distance;
                    hitBlockIndex = i;
                }
            }
        }

        if (hitBlockIndex != -1) {
            BoundingBox hitBlockBox = {
                Vector3SubtractValue(blocks[hitBlockIndex].position, 0.5f),
                Vector3AddValue(blocks[hitBlockIndex].position, 0.5f)
            };
            RayCollision hitInfo = GetRayCollisionBox(ray, hitBlockBox);
            Vector3 roundedNormal = { roundf(hitInfo.normal.x), roundf(hitInfo.normal.y), roundf(hitInfo.normal.z) };
            ghostBlockPos = Vector3Add(blocks[hitBlockIndex].position, roundedNormal);
            showGhostBlock = true;
        } else {
            BoundingBox groundBox = { (Vector3){-0.5f, -0.5f, -0.5f}, (Vector3){BOARD_SIZE-0.5f, 0.5f, BOARD_SIZE-0.5f}};
            RayCollision groundHit = GetRayCollisionBox(ray, groundBox);
            if (groundHit.hit) {
                ghostBlockPos = (Vector3){ roundf(groundHit.point.x), 0.5f, roundf(groundHit.point.z) };
                showGhostBlock = true;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && showGhostBlock && mouseCaptured) {
            bool occupied = false;
            BoundingBox ghostBox = {
                Vector3SubtractValue(ghostBlockPos, 0.5f),
                Vector3AddValue(ghostBlockPos, 0.5f)
            };
            BoundingBox playerBox = {
                (Vector3){ camera.position.x - PLAYER_RADIUS, camera.position.y - PLAYER_HEIGHT / 2.0f, camera.position.z - PLAYER_RADIUS },
                (Vector3){ camera.position.x + PLAYER_RADIUS, camera.position.y + PLAYER_HEIGHT / 2.0f, camera.position.z + PLAYER_RADIUS }
            };

            if (CheckAABBCollision(ghostBox, playerBox)) {
                occupied = true;
            } else {
                for (int i = 0; i < blockCount; i++) {
                    if (blocks[i].active &&
                        fabs(blocks[i].position.x - ghostBlockPos.x) < 0.1f &&
                        fabs(blocks[i].position.y - ghostBlockPos.y) < 0.1f &&
                        fabs(blocks[i].position.z - ghostBlockPos.z) < 0.1f) {
                        occupied = true;
                        break;
                    }
                }
            }

            if (!occupied) {
                if (blockCount >= blockCapacity) {
                    blockCapacity *= 2;
                    blocks = (Block*)realloc(blocks, blockCapacity * sizeof(Block));
                }
                blocks[blockCount].position = ghostBlockPos;
                blocks[blockCount].active = true;
                blocks[blockCount].type = selectedBlockType;
                blockCount++;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hitBlockIndex != -1 && mouseCaptured) {
            if (fadingBlockCount < fadingBlockCapacity) {
                fadingBlocks[fadingBlockCount].position = blocks[hitBlockIndex].position;
                fadingBlocks[fadingBlockCount].type = blocks[hitBlockIndex].type;
                fadingBlocks[fadingBlockCount].fadeTimer = FADE_TIME;
                fadingBlocks[fadingBlockCount].active = true;
                fadingBlockCount++;
            }
            blocks[hitBlockIndex].active = false;
            _gc();
            hitBlockIndex = -1;
        }

        skip_mouse_input:

        for (int i = 0; i < fadingBlockCount; i++) {
            if (fadingBlocks[i].active) {
                fadingBlocks[i].fadeTimer -= deltaTime;
                if (fadingBlocks[i].fadeTimer <= 0) {
                    fadingBlocks[i].active = false;
                }
            }
        }
        _gc();

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        rlDisableBackfaceCulling();
        DrawModel(groundModel, (Vector3){(float)BOARD_SIZE/2.0f-0.5f, 0.0f, (float)BOARD_SIZE/2.0f-0.5f}, 1.0f, WHITE);
        rlEnableBackfaceCulling();

        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                Vector3 pos = blocks[i].position;
                blockModel.materials[0] = *blockMaterials[blocks[i].type];
                DrawModel(blockModel, pos, 1.0f, WHITE);
            }
        }

        rlDisableBackfaceCulling();
        for (int i = 0; i < fadingBlockCount; i++) {
            if (fadingBlocks[i].active) {
                float alpha = fadingBlocks[i].fadeTimer / FADE_TIME;
                Color fadeColor = (Color){255, 255, 255, (unsigned char)(alpha * 255)};
                blockModel.materials[0] = *blockMaterials[fadingBlocks[i].type];
                DrawModel(blockModel, fadingBlocks[i].position, 1.0f, fadeColor);
            }
        }
        rlEnableBackfaceCulling();

        if (showGhostBlock && mouseCaptured) {
            float t = fmodf(GetTime() * GHOST_BLOCK_SPEED, 1.0f);
            float triangle = (t < 0.5f) ? (t * 2.0f) : (1.0f - (t - 0.5f) * 2.0f);
            float alpha = GHOST_BLOCK_MIN_ALPHA + (GHOST_BLOCK_MAX_ALPHA - GHOST_BLOCK_MIN_ALPHA) * triangle;

            Color ghostColor = (Color){ 255, 255, 255, (unsigned char)(alpha * 255) };
            ghostBlockModel.materials[0] = *blockMaterials[selectedBlockType];
            DrawModel(ghostBlockModel, ghostBlockPos, 1.0f, ghostColor);
        }

        EndMode3D();

        DrawBlockPreview(blockTextures[selectedBlockType]);

        DrawText(TextFormat("FPS: %i", displayedFPS), 10, 10, FPS_TEXT_SIZE, WHITE);
        DrawText(TextFormat("Blocks: %i", blockCount), 10, 10 + FPS_TEXT_SIZE, FPS_TEXT_SIZE, WHITE);

        if (mouseCaptured) {
            DrawLine(screenWidth / 2 - CROSSHAIR_SIZE, screenHeight / 2, screenWidth / 2 + CROSSHAIR_SIZE, screenHeight / 2, WHITE);
            DrawLine(screenWidth / 2, screenHeight / 2 - CROSSHAIR_SIZE, screenWidth / 2, screenHeight / 2 + CROSSHAIR_SIZE, WHITE);
        }

        EndDrawing();
    }

    UnloadModel(blockModel);
    UnloadModel(ghostBlockModel);
    UnloadModel(groundModel);
    UnloadTexture(grassTexture);
    UnloadTexture(stoneTexture);
    UnloadTexture(dirtTexture);
    UnloadTexture(woodTexture);
    UnloadMaterial(stoneMaterial);
    UnloadMaterial(grassMaterial);
    UnloadMaterial(dirtMaterial);
    UnloadMaterial(woodMaterial);
    UnloadMaterial(groundMaterial);

    free(blocks);
    free(fadingBlocks);

    EnableCursor();
    CloseWindow();
    return 0;
}
