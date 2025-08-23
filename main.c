#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define TILE_SIZE 1.0f
#define BOARD_SIZE 100

#define GRAVITY -0.01f
#define JUMP_FORCE 0.2f
#define RESPAWN_Y_THRESHOLD -30.0f

#define CAMERA_HEIGHT 1.9f
#define CAMERA_SPEED 0.1f
#define MOUSE_SENSITIVITY 0.003f
#define PLAYER_RADIUS 0.3f
#define PLAYER_HEIGHT 1.9f
#define SPAWN_POSITION (Vector3){ 50.0f, CAMERA_HEIGHT, 50.0f }

#define GHOST_BLOCK_MIN_ALPHA 0.3f
#define GHOST_BLOCK_MAX_ALPHA 0.7f
#define GHOST_BLOCK_SPEED 2.0f
#define CUBE_WIRE_OFFSET 1.001f

#define GROUND_COLOR_1 DARKGREEN
#define GROUND_COLOR_2 GREEN
#define GROUND_THICKNESS 0.1f

#define BLOCK_COLOR GRAY
#define GHOST_BLOCK_COLOR WHITE

#define MAX_DEBRIS_PIECES 5
#define MAX_TOTAL_DEBRIS (1000 * MAX_DEBRIS_PIECES)
#define DEBRIS_SIZE 0.2f
#define DEBRIS_FALL_THRESHOLD -5.0f

#define CROSSHAIR_SIZE 10
#define FPS_TEXT_SIZE 20

typedef struct {
    Vector3 position;
    bool active;
} Block;

typedef struct {
    Vector3 position;
    Vector3 velocity;
    bool active;
} Debris;

Block* blocks = NULL;
int blockCount = 0;
int blockCapacity = 0;

Debris* debris = NULL;
int debrisCount = 0;
int debrisCapacity = 0;

bool CheckPlayerCollision(Vector3 playerPos, Block* blocks, int blockCount);
float FindGroundLevel(Vector3 position, Block* blocks, int blockCount);
bool IsPlayerSupported(Vector3 playerPos, Block* blocks, int blockCount);
void _gc();

bool CheckPlayerCollision(Vector3 playerPos, Block* blocks, int blockCount) {

    Vector3 playerFeet = { playerPos.x, playerPos.y - PLAYER_HEIGHT / 2, playerPos.z };
    Vector3 playerHead = { playerPos.x, playerPos.y + PLAYER_HEIGHT / 2, playerPos.z };

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            Vector3 blockMin = { blocks[i].position.x - 0.5f, blocks[i].position.y - 0.5f, blocks[i].position.z - 0.5f };
            Vector3 blockMax = { blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f };

            bool horizontalOverlap = (fabs(playerPos.x - blocks[i].position.x) < (0.5f + PLAYER_RADIUS)) &&
                                     (fabs(playerPos.z - blocks[i].position.z) < (0.5f + PLAYER_RADIUS));

            bool verticalOverlap = (playerHead.y >= blockMin.y) && (playerFeet.y <= blockMax.y);

            if (horizontalOverlap && verticalOverlap) {
                return true;
            }
        }
    }
    return false;
}

bool CheckHorizontalCollision(Vector3 playerPos, Block* blocks, int blockCount) {
    Vector3 playerFeet = { playerPos.x, playerPos.y - PLAYER_HEIGHT / 2, playerPos.z };

    for (int i = 0; i < blockCount; i++) {
        if (blocks[i].active) {
            Vector3 blockMin = { blocks[i].position.x - 0.5f, blocks[i].position.y - 0.5f, blocks[i].position.z - 0.5f };
            Vector3 blockMax = { blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f };

            bool horizontalOverlap = (fabs(playerPos.x - blocks[i].position.x) < (0.5f + PLAYER_RADIUS)) &&
                                     (fabs(playerPos.z - blocks[i].position.z) < (0.5f + PLAYER_RADIUS));

            bool verticalOverlap = playerFeet.y <= blockMax.y;

            if (horizontalOverlap && verticalOverlap) {
                return true;
            }
        }
    }
    return false;
}

float FindGroundLevel(Vector3 position, Block* blocks, int blockCount) {
    float groundLevel = 0.0f;

    if (position.x >= 0 && position.x < BOARD_SIZE && position.z >= 0 && position.z < BOARD_SIZE) {
        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                float dx = fabs(position.x - blocks[i].position.x);
                float dz = fabs(position.z - blocks[i].position.z);

                if (dx < (0.5f + PLAYER_RADIUS) && dz < (0.5f + PLAYER_RADIUS)) {
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
    if (playerPos.x >= 0 && playerPos.x < BOARD_SIZE && playerPos.z >= 0 && playerPos.z < BOARD_SIZE) {
        float groundLevel = FindGroundLevel(playerPos, blocks, blockCount);
        float expectedHeight = groundLevel + CAMERA_HEIGHT;

        return fabs(playerPos.y - expectedHeight) < 0.15f;
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

    int newDebrisCount = 0;
    for (int i = 0; i < debrisCount; i++) {
        if (debris[i].active) {
            debris[newDebrisCount] = debris[i];
            newDebrisCount++;
        }
    }
    debrisCount = newDebrisCount;
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(800, 600, "box");
    SetTargetFPS(60);

    int screenWidth = 800;
    int screenHeight = 600;

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

    blockCapacity = 100;
    blocks = (Block*)malloc(blockCapacity * sizeof(Block));

    debrisCapacity = MAX_TOTAL_DEBRIS;
    debris = (Debris*)malloc(debrisCapacity * sizeof(Debris));

    Texture2D grassTexture = LoadTexture("grass.jpg");
    Texture2D stoneTexture = LoadTexture("stone.jpg");

    Model groundModel = LoadModelFromMesh(GenMeshCube(TILE_SIZE, GROUND_THICKNESS, TILE_SIZE));
    Model blockModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));
    Model debrisModel = LoadModelFromMesh(GenMeshCube(DEBRIS_SIZE, DEBRIS_SIZE, DEBRIS_SIZE));

    Model ghostBlockModel = LoadModelFromMesh(GenMeshCube(1.0f, 1.0f, 1.0f));

    Material grassMaterial = LoadMaterialDefault();
    grassMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = grassTexture;

    Material stoneMaterial = LoadMaterialDefault();
    stoneMaterial.maps[MATERIAL_MAP_DIFFUSE].texture = stoneTexture;

    Material ghostMaterial = LoadMaterialDefault();

    groundModel.materials[0] = grassMaterial;
    blockModel.materials[0] = stoneMaterial;
    debrisModel.materials[0] = stoneMaterial;

    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            screenWidth = GetScreenWidth();
            screenHeight = GetScreenHeight();

            camera.fovy = 60.0f;
        }

        if (IsKeyPressed(KEY_BACKSPACE)) {
            mouseCaptured = !mouseCaptured;
            if (mouseCaptured) {
                DisableCursor();
            } else {
                EnableCursor();
            }
        }

        if (IsKeyPressed(KEY_F11)) {
            ToggleFullscreen();
            if (mouseCaptured) {
                SetMousePosition(GetScreenWidth() / 2, GetScreenHeight() / 2);
            }
        }

        if (skipNextClick && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
            skipNextClick = false;
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

        Vector3 newPosition = Vector3Add(camera.position, Vector3Scale(move, CAMERA_SPEED));
        newPosition.y = camera.position.y;

        if (!CheckHorizontalCollision(newPosition, blocks, blockCount)) {
            camera.position.x = newPosition.x;
            camera.position.z = newPosition.z;
        }

        if (isGrounded && IsKeyPressed(KEY_SPACE)) {
            Vector3 testPos = camera.position;
            testPos.y += JUMP_FORCE * 15;

            if (!CheckPlayerCollision(testPos, blocks, blockCount)) {
                velocityY = JUMP_FORCE;
                isGrounded = false;
            }
        }

        if (isGrounded && !IsPlayerSupported(camera.position, blocks, blockCount)) {
            isGrounded = false;
            velocityY = 0.0f;
        }

        if (!isGrounded) {
            velocityY += GRAVITY;
            float newY = camera.position.y + velocityY;
            Vector3 testPos = camera.position;
            testPos.y = newY;

            if (!CheckPlayerCollision(testPos, blocks, blockCount)) {
                camera.position.y = newY;

                if (IsPlayerSupported(testPos, blocks, blockCount)) {
                    float groundLevel = FindGroundLevel(camera.position, blocks, blockCount);
                    if (groundLevel > -999.0f) {
                        float expectedCameraHeight = groundLevel + CAMERA_HEIGHT;
                        camera.position.y = expectedCameraHeight;
                        velocityY = 0.0f;
                        isGrounded = true;
                    }
                }
            } else {
                float groundLevel = FindGroundLevel(camera.position, blocks, blockCount);
                if (groundLevel > -999.0f) {
                    camera.position.y = groundLevel + CAMERA_HEIGHT;
                    velocityY = 0.0f;
                    isGrounded = true;
                }
            }
        }

        if (camera.position.y < RESPAWN_Y_THRESHOLD) {
            camera.position = SPAWN_POSITION;
            velocityY = 0.0f;
            isGrounded = true;
        }

        camera.target = Vector3Add(camera.position, forward);

        Ray ray = GetMouseRay((Vector2){screenWidth / 2, screenHeight / 2}, camera);

        Vector3 cursorBlock;
        bool validHit = false;

        RayCollision closestBlockHit = { 0 };
        closestBlockHit.distance = 10000.0f;
        int hitBlockIndex = -1;

        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                BoundingBox blockBox = (BoundingBox){
                    (Vector3){ blocks[i].position.x - 0.5f, blocks[i].position.y - 0.5f, blocks[i].position.z - 0.5f },
                    (Vector3){ blocks[i].position.x + 0.5f, blocks[i].position.y + 0.5f, blocks[i].position.z + 0.5f }
                };
                RayCollision hitInfo = GetRayCollisionBox(ray, blockBox);
                if (hitInfo.hit && hitInfo.distance < closestBlockHit.distance) {
                    closestBlockHit = hitInfo;
                    hitBlockIndex = i;
                }
            }
        }

        if (closestBlockHit.hit) {
            Vector3 normal = closestBlockHit.normal;
            Vector3 gridNormal = {roundf(normal.x), roundf(normal.y), roundf(normal.z)};

            cursorBlock = Vector3Add(blocks[hitBlockIndex].position, gridNormal);
            validHit = true;
        } else {
            if (ray.direction.y < 0) {
                float t = -ray.position.y / ray.direction.y;
                if (t > 0) {
                    Vector3 hitPoint = Vector3Add(ray.position, Vector3Scale(ray.direction, t));

                    int tileX = (int)roundf(hitPoint.x);
                    int tileZ = (int)roundf(hitPoint.z);

                    if (tileX >= 0 && tileX < BOARD_SIZE && tileZ >= 0 && tileZ < BOARD_SIZE) {
                        float highestY = 0.0f;
                        for (int i = 0; i < blockCount; i++) {
                            if (blocks[i].active &&
                                fabs(blocks[i].position.x - tileX) < 0.1f &&
                                fabs(blocks[i].position.z - tileZ) < 0.1f) {
                                float blockTop = blocks[i].position.y + 0.5f;
                                if (blockTop > highestY) {
                                    highestY = blockTop;
                                }
                            }
                        }
                        cursorBlock.x = (float)tileX;
                        cursorBlock.y = highestY + 0.5f;
                        cursorBlock.z = (float)tileZ;
                        validHit = true;
                    }
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && validHit && mouseCaptured && !skipNextClick) {
            bool occupied = false;
            Vector3 playerFeetBlock = { roundf(camera.position.x), roundf(camera.position.y - 1.0f), roundf(camera.position.z) };
            Vector3 playerHeadBlock = { roundf(camera.position.x), roundf(camera.position.y), roundf(camera.position.z) };

            if (fabs(cursorBlock.x - playerFeetBlock.x) < 0.1f &&
                fabs(cursorBlock.y - playerFeetBlock.y) < 0.1f &&
                fabs(cursorBlock.z - playerFeetBlock.z) < 0.1f) {
                occupied = true;
            }
            if (!occupied && fabs(cursorBlock.x - playerHeadBlock.x) < 0.1f &&
                fabs(cursorBlock.y - playerHeadBlock.y) < 0.1f &&
                fabs(cursorBlock.z - playerHeadBlock.z) < 0.1f) {
                occupied = true;
            }

            for (int i = 0; i < blockCount; i++) {
                if (blocks[i].active &&
                    fabs(blocks[i].position.x - cursorBlock.x) < 0.1f &&
                    fabs(blocks[i].position.y - cursorBlock.y) < 0.1f &&
                    fabs(blocks[i].position.z - cursorBlock.z) < 0.1f) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                if (blockCount >= blockCapacity) {
                    blockCapacity *= 2;
                    blocks = (Block*)realloc(blocks, blockCapacity * sizeof(Block));
                }
                blocks[blockCount].position = cursorBlock;
                blocks[blockCount].active = true;
                blockCount++;
            }
        }

        if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT) && hitBlockIndex != -1 && mouseCaptured) {
            Vector3 brokenBlockPos = blocks[hitBlockIndex].position;
            blocks[hitBlockIndex].active = false;
            _gc();

            for (int i = 0; i < MAX_DEBRIS_PIECES; i++) {
                if (debrisCount < debrisCapacity) {
                    debris[debrisCount].position = brokenBlockPos;
                    debris[debrisCount].active = true;

                    debris[debrisCount].velocity.x = GetRandomValue(-100, 100) / 1000.0f;
                    debris[debrisCount].velocity.y = GetRandomValue(50, 200) / 1000.0f;
                    debris[debrisCount].velocity.z = GetRandomValue(-100, 100) / 1000.0f;

                    debrisCount++;
                }
            }
            hitBlockIndex = -1;
        }

        for (int i = 0; i < debrisCount; i++) {
            if (debris[i].active) {
                debris[i].velocity.y += GRAVITY;
                debris[i].position = Vector3Add(debris[i].position, debris[i].velocity);

                if (debris[i].position.y < DEBRIS_FALL_THRESHOLD) {
                    debris[i].active = false;
                }
            }
        }
        _gc();

        BeginDrawing();
        ClearBackground(SKYBLUE);

        BeginMode3D(camera);

        for (int x = 0; x < BOARD_SIZE; x++) {
            for (int z = 0; z < BOARD_SIZE; z++) {
                Vector3 tilePos = { x * TILE_SIZE, 0.0f, z * TILE_SIZE };

                DrawModel(groundModel, tilePos, 1.0f, WHITE);
            }
        }

        for (int i = 0; i < blockCount; i++) {
            if (blocks[i].active) {
                Vector3 pos = blocks[i].position;

                DrawModel(blockModel, pos, 1.0f, WHITE);
            }
        }

        for (int i = 0; i < debrisCount; i++) {
            if (debris[i].active) {

                DrawModel(debrisModel, debris[i].position, 1.0f, WHITE);
            }
        }

        if (validHit && mouseCaptured) {
            float t = fmodf(GetTime() * GHOST_BLOCK_SPEED, 1.0f);
            float triangle = (t < 0.5f) ? (t * 2.0f) : (1.0f - (t - 0.5f) * 2.0f);
            float alpha = GHOST_BLOCK_MIN_ALPHA + (GHOST_BLOCK_MAX_ALPHA - GHOST_BLOCK_MIN_ALPHA) * triangle;

            Color ghostColor = (Color){ 255, 255, 255, (unsigned char)(alpha * 255) };

            DrawModel(ghostBlockModel, cursorBlock, 1.0f, ghostColor);
        }

        EndMode3D();

        DrawText(TextFormat("FPS: %i", GetFPS()), 10, 10, FPS_TEXT_SIZE, WHITE);
        DrawText(TextFormat("Blocks: %i", blockCount), 10, 10 + FPS_TEXT_SIZE, FPS_TEXT_SIZE, WHITE);

        if (mouseCaptured) {
            DrawLine(screenWidth / 2 - CROSSHAIR_SIZE, screenHeight / 2, screenWidth / 2 + CROSSHAIR_SIZE, screenHeight / 2, WHITE);
            DrawLine(screenWidth / 2, screenHeight / 2 - CROSSHAIR_SIZE, screenWidth / 2, screenHeight / 2 + CROSSHAIR_SIZE, WHITE);
        }

        EndDrawing();
    }

    UnloadModel(groundModel);
    UnloadModel(blockModel);
    UnloadModel(debrisModel);
    UnloadModel(ghostBlockModel); 
    UnloadTexture(grassTexture);
    UnloadTexture(stoneTexture);
    UnloadMaterial(grassMaterial);
    UnloadMaterial(stoneMaterial);
    UnloadMaterial(ghostMaterial); 

    free(blocks);
    free(debris);

    EnableCursor();
    CloseWindow();
    return 0;
}
