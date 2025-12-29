#include "Globals.h"
#include "Application.h"
#include "ModulePlayer.h"
#include "ModulePhysics.h"
#include "ModuleGame.h"
#include "ModuleMap.h"
#include "CarProperties.h"
#include "ModuleRender.h"
#include<algorithm>
ModulePlayer::ModulePlayer(Application* app, bool start_enabled) : Module(app, start_enabled)
{
}

ModulePlayer::~ModulePlayer()
{
}

// Load assets
bool ModulePlayer::Start()
{
	LOG("Loading player");
	texture = LoadTexture("Assets-racing/Textures/Car1.png");

   
    int frameWidth = 26;
    int frameHeight = 43;
    x = App->map->playerSpawnPoint.x;
    y = App->map->playerSpawnPoint.y;

    if (x == 0 && y == 0) {
        x = 100; // 默认值
        y = 300;
        LOG("Warning: No PlayerStart found in map, using default.");
    }

    pbody = App->physics->CreateRectangle(x, y, frameWidth, frameHeight, 1, 0xFFFF);

    this->width = frameWidth;
    this->height = frameHeight;

    if (pbody != nullptr)
    {
        pbody->listener = this;
        // 使用 CarStats 中定义的统一数值
        pbody->body->SetLinearDamping(CarStats::LINEAR_DAMPING);
        pbody->body->SetAngularDamping(CarStats::ANGULAR_DAMPING);

        // 获取并修改 Fixture 属性 (影响质量和碰撞反应)
        b2Fixture* fixture = pbody->body->GetFixtureList();
        if (fixture) {
            fixture->SetDensity(CarStats::DENSITY);
            fixture->SetFriction(CarStats::FRICTION);
            fixture->SetRestitution(CarStats::RESTITUTION);
            pbody->body->ResetMassData(); // 修改密度后必须重置
        }
    }

    // 使用 CarStats 中定义的动力数值
    speed = CarStats::ACCELERATION;
    turn_speed = CarStats::STEERING_SPEED;


    currentMaxSpeed = CarStats::MAX_SPEED; // 初始速度
    boostTimer = 0;

    currentBoostCharge = 0.0f;

    // 这里设定“1/4圈”的距离。
    // 注意：Box2D的单位是米。你需要根据你的地图大小调整这个数值。
    // 如果一圈大概是 2000米，那这里设为 500.0f。
    // 你需要运行游戏测试一下，跑完1/4路程时看日志或UI调整这个值。
    maxBoostCharge = 1500.0f;
	return true;
}

// Unload assets
bool ModulePlayer::CleanUp()
{
	LOG("Unloading player");
    UnloadTexture(texture);
	return true;
}
update_status ModulePlayer::Update()
{
    if (App->game != nullptr && App->game->game_over == true) return UPDATE_CONTINUE;

    if (App->game != nullptr && App->game->game_over == true) {
        if (pbody != nullptr && pbody->body != nullptr) {
            pbody->body->SetLinearVelocity({ 0, 0 });
            pbody->body->SetAngularVelocity(0);
        }
        return UPDATE_CONTINUE;
    }

    if (pbody != nullptr)
    {
        // 1. 处理加速计时逻辑
        if (boostTimer > 0) {
            boostTimer--;
            if (boostTimer == 0) {
                currentMaxSpeed = CarStats::MAX_SPEED; // 时间到，恢复原始速度
                LOG("Speed normal");
            }
        }
        float currentSpeed = pbody->body->GetLinearVelocity().Length();

        if (boostTimer <= 0 && currentBoostCharge < maxBoostCharge) {
            currentBoostCharge += currentSpeed * 0.5f;

            if (currentBoostCharge>maxBoostCharge) {
                currentBoostCharge = maxBoostCharge;
                LOG("Boost Ready !");
            }
        }







        // 2. 使用 currentMaxSpeed 而不是固定值
        // 如果 currentMaxSpeed 还没初始化，就在 Start 里设置 currentMaxSpeed = CarStats::MAX_SPEED
        float maxSpeed = currentMaxSpeed;






        b2Body* b = pbody->body;

        // 1. OBTENER DIRECCI?N Y VELOCIDAD
        float currentAngle = b->GetAngle();
        // IMPORTANTE: Esta fórmula debe ser igual en la IA
        b2Vec2 forwardDir = { (float)sin(currentAngle), (float)-cos(currentAngle) };
        b2Vec2 currentVel = b->GetLinearVelocity();
        float forwardSpeed = currentVel.x * forwardDir.x + currentVel.y * forwardDir.y;

        // 2. FRICCI?N LATERAL (SIN DERRAPE)
        // Sincronizado con KillOrthogonalVelocity de la IA
        b2Vec2 lateralCorrection = { forwardDir.x * forwardSpeed, forwardDir.y * forwardSpeed };
        b->SetLinearVelocity(lateralCorrection);

        // 3. GIRO (STEERING)
        float targetRotVelocity = 0.0f;
        float maxTurnSpeed = CarStats::STEERING_SPEED;

        if (abs(forwardSpeed) > 0.5f) // Mínimo de velocidad para girar
        {
            if (IsKeyDown(KEY_LEFT) || IsKeyDown(KEY_A)) targetRotVelocity = -maxTurnSpeed;
            else if (IsKeyDown(KEY_RIGHT) || IsKeyDown(KEY_D)) targetRotVelocity = maxTurnSpeed;

            if (forwardSpeed < -0.1f) targetRotVelocity *= -1; // Invertir en reversa
        }

        // Sincroniza este valor (0.2f) con la IA si quieres que giren con la misma agilidad
        float turnSmoothing = 0.2f;
        float currentRot = b->GetAngularVelocity();
        float newRot = currentRot + (targetRotVelocity - currentRot) * turnSmoothing;
        b->SetAngularVelocity(newRot);

        // 4. ACELERACI?N
    /*    float maxSpeed = CarStats::MAX_SPEED;*/

        if (IsKeyPressed(KEY_SPACE) && boostTimer <= 0&&currentBoostCharge>=maxBoostCharge) // IsKeyPressed para que solo se active una vez por pulsaci髇
        {
            boostTimer = 120; // 2 segundos a 60 FPS
            currentMaxSpeed = CarStats::MAX_SPEED * 1.5f; // Aumentamos la velocidad m醲ima (ejemplo: x1.5)
            currentBoostCharge = 0.0f;
            LOG("BOOST ACTIVADO! Change reset");
        }

        if (IsKeyDown(KEY_UP) || IsKeyDown(KEY_W))
        {
            if (forwardSpeed < maxSpeed) {
                // 'speed' ya es CarStats::ACCELERATION (asignado en Start)
                b2Vec2 force = { forwardDir.x * speed, forwardDir.y * speed };
                b->ApplyForceToCenter(force, true);
            }
        }

        // 5. FRENADO Y REVERSA
        if (IsKeyDown(KEY_DOWN) || IsKeyDown(KEY_S))
        {
            if (forwardSpeed > -maxSpeed * 0.5f) {
                b2Vec2 force = { forwardDir.x * -speed * 0.5f, forwardDir.y * -speed * 0.5f };
                b->ApplyForceToCenter(force, true);
            }
        }

        // 6. FRICCI?N NATURAL (Cuando no aceleras)
        if (!IsKeyDown(KEY_UP) && !IsKeyDown(KEY_DOWN) && !IsKeyDown(KEY_SPACE)) {
            b2Vec2 v = b->GetLinearVelocity();
            v.x *= 0.98f; // Sincroniza este 0.98f con la IA
            v.y *= 0.98f;
            b->SetLinearVelocity(v);
        }
        //Get player position
        int px;
        int py;
        pbody->GetPhysicPosition(px,py);
        // --- 镜头跟随与限制逻辑 (Camera Clamping) ---

              // 1. 获取地图总尺寸 (Tiles * TileSize)
              // 注意：请检查你的 ModuleMap.h，确认结构体实例名叫 'mapData' 还是 'data'
        int mapWidth = App->map->mapData.width * App->map->mapData.tileWidth;
        int mapHeight = App->map->mapData.height * App->map->mapData.tileHeight;

        // 2. 计算当前视野的一半大小 (考虑 Zoom)
        // 如果 Zoom=2.0，屏幕宽800，那么视野宽只有400，中心点是200
        float zoom = App->renderer->camera.zoom;
        float halfScreenW = (GetScreenWidth() / 2.0f) / zoom;
        float halfScreenH = (GetScreenHeight() / 2.0f) / zoom;

        // 3. 计算允许摄像机移动的最小和最大坐标
        // 摄像机中心最小不能小于半个屏幕宽
        float minX = halfScreenW;
        float minY = halfScreenH;
        // 摄像机中心最大不能超过 地图宽 - 半个屏幕宽
        float maxX = mapWidth - halfScreenW;
        float maxY = mapHeight - halfScreenH;

        // 4. 限制目标点 (Clamping)
        // 使用 std::clamp 或者手写 if/else
        float targetX = (float)px;
        float targetY = (float)py;

        // 如果地图比屏幕还小，就固定在地图中心，防止抖动
        if (mapWidth < GetScreenWidth() / zoom) {
            targetX = mapWidth / 2.0f;
        }
        else {
            if (targetX < minX) targetX = minX;
            if (targetX > maxX) targetX = maxX;
        }

        if (mapHeight < GetScreenHeight() / zoom) {
            targetY = mapHeight / 2.0f;
        }
        else {
            if (targetY < minY) targetY = minY;
            if (targetY > maxY) targetY = maxY;
        }

        // 5. 应用目标
        App->renderer->camera.target = { targetX, targetY };

        // 偏移量始终保持屏幕中心
        App->renderer->camera.offset = { (float)GetScreenWidth() / 2.0f, (float)GetScreenHeight() / 2.0f };
    }

    

    return UPDATE_CONTINUE;
}



update_status ModulePlayer::PostUpdate()
{
    if (pbody != nullptr)
    {
        // --- RENDERIZADO (RAYLIB) ---

        // 1. Obtener posici髇 actualizada de las f韘icas
        int posX, posY;
        pbody->GetPhysicPosition(posX, posY);

        // 2. Obtener rotaci髇
        float rotationDegrees = pbody->GetRotation() * RAD2DEG;

        // 3. Dibujar
        Rectangle sourceRec = { 0.0f, 0.0f, (float)this->width,(float)this->height };
        Rectangle destRec = { (float)posX, (float)posY, (float)this->width, (float)this->height };
        Vector2 origin = { (float)this->width / 2, (float)this->height / 2 };

        DrawTexturePro(texture, sourceRec, destRec, origin, rotationDegrees, WHITE);

        EndMode2D();
        //UI boost with space
        int barWidth = 200;
        int barHeight = 20;
        int screenW = GetScreenWidth();
        int magin = 80;
        int barX = screenW - barWidth - magin;
        int barY = 30;

        float percentage = currentBoostCharge / maxBoostCharge;

        if (percentage > 1.0f)percentage = 1.0f;

        DrawRectangle(barX, barY, barWidth, barHeight, Fade(DARKGRAY, 0.8f));
        DrawRectangleLines(barX,barY,barWidth,barHeight,WHITE);//margen

        Color barColor = YELLOW;

        if (percentage >= 1.0f) {
            barColor = GREEN;

            DrawText("Boost Ready [SPACE]", barX, barY + 25, 10, GREEN);
        }
        
        DrawRectangle(barX, barY, (int)(barWidth * percentage), barHeight, barColor);

        if (boostTimer > 0) {
            DrawText("BOOSTING!!", barX, barY + 25, 10, RED);

            DrawRectangle(barX,barY,barWidth,barHeight,RED);
        }


        BeginMode2D(App->renderer->camera);


    }

    return UPDATE_CONTINUE;
}

// ModulePlayer.cpp
void ModulePlayer::OnCollision(PhysBody* bodyA, PhysBody* bodyB)
{
    // bodyB 是玩家撞到的东西
    if (bodyB != nullptr && bodyB->ptype == BodyType::BOOST)
    {
        boostTimer = 60; // 3秒加速 (假设60fps)
        currentMaxSpeed = CarStats::MAX_SPEED +3.0f; // 速度翻倍
        LOG("BOOST ACTIVE!");
    }
}