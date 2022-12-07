#include <stdio.h>
#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include "../submodules/cJSON/cJSON.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#endif

#define CHECKPOINTS 5

Shader _shader;

struct Checkpoint{
    Vector3 pos;
    Vector3 normal;
    Quaternion surfaceRotation;
    Quaternion rotation;
}typedef Checkpoint;

struct Kart{
    Model model;
    BoundingBox box;
    Vector3 position;
    Quaternion rotation;
    Quaternion wantedRotation;
    Quaternion surfaceRotation;
    Vector3 velocity;
    Vector3 rotVel;
    Vector3 size;
    Vector3 normal;
    float boost;
    bool grounded;
    bool stopped;
    float airTime;
    Checkpoint checkpoints[CHECKPOINTS];
    int checkpointIndex;
}typedef Kart;

Color ColorBlend(Color a, Color b, float factor)
{
    Color col;
    col.r = Lerp(a.r, b.r, factor);
    col.g = Lerp(a.g, b.g, factor);
    col.b = Lerp(a.b, b.b, factor);
    col.a = Lerp(a.a, b.a, factor);
    return col;
}

Vector3 GetSizeFromBoundingBox(BoundingBox box)
{
    Vector3 var = Vector3Subtract(box.max, box.min);
    return var;
}

void ResetKart(Kart * kart)
{
    kart->rotation = QuaternionFromEuler(0,0,0);
    kart->wantedRotation = kart->rotation;
    kart->surfaceRotation = kart->rotation;
    kart->normal = (Vector3){0, 1, 0};
    kart->boost = -5;
    kart->airTime = 0;
    kart->rotVel = (Vector3){0};
    kart->velocity = (Vector3){0};
}

Kart LoadKart(char * filename, Vector3 position, Vector3 size)
{
    Kart obj = {0};
    obj.model = LoadModel(filename);
    obj.box = GetMeshBoundingBox(obj.model.meshes[0]);
    obj.position = position;
    obj.size = size;
    ResetKart(&obj);

    for(int cp = 0; cp < CHECKPOINTS; cp++)
    {
        obj.checkpoints[cp].normal = (Vector3){0, 1, 0};
        obj.checkpoints[cp].rotation = QuaternionFromEuler(0,0,0);
        obj.checkpoints[cp].surfaceRotation = QuaternionFromEuler(0,0,0);
    }

    return obj;
}

void DrawKart(Kart obj)
{
    Vector3 axis;
    float angle;

    Quaternion rotation = obj.wantedRotation;

    rotation = QuaternionMultiply(obj.surfaceRotation, rotation);
    
    QuaternionToAxisAngle(rotation, &axis, &angle);
    DrawModelEx(obj.model, obj.position, axis, angle*(360/(PI*2)), obj.size, WHITE);
}

void KartPhysics(Kart * obj, float time)
{
    if(Vector3Distance(obj->checkpoints[obj->checkpointIndex].pos, obj->position) > 6 && obj->grounded)
    {
        Checkpoint cp;

        cp.pos = obj->position;
        cp.normal = obj->normal;
        cp.rotation = obj->rotation;
        cp.surfaceRotation = obj->surfaceRotation;
        obj->checkpointIndex = (obj->checkpointIndex+1) % CHECKPOINTS;
        obj->checkpoints[obj->checkpointIndex] = cp;
    }

    if(obj->airTime < -1.5)
    {
        obj->checkpointIndex = (obj->checkpointIndex+CHECKPOINTS-1)%CHECKPOINTS;
        Checkpoint cp = obj->checkpoints[obj->checkpointIndex];
        ResetKart(obj);
        obj->position = Vector3Add(cp.pos, Vector3Scale(cp.normal, 2));
        obj->rotation = cp.rotation;
        obj->wantedRotation = cp.rotation;
        obj->surfaceRotation = cp.surfaceRotation;
        obj->normal = cp.normal;
        printf("resetting to checkout\n");
    }

    if(obj->stopped && obj->velocity.z > 0)
        obj->velocity.z = 0;
    
    Quaternion velocityRotation = QuaternionMultiply(obj->surfaceRotation, obj->rotation);

    obj->position = Vector3Add(obj->position, Vector3RotateByQuaternion(Vector3Scale(obj->velocity, time), velocityRotation));


    Quaternion newRot = QuaternionMultiply(obj->wantedRotation, QuaternionFromEuler(obj->rotVel.x, obj->rotVel.y, obj->rotVel.z));

    float speed = obj->velocity.z;

    if(speed > 1)
        speed -= 1;
    else if(speed < -1)
        speed += 1;
    else
        speed = 0;

    if(obj->airTime < -0.2)
    {
        obj->velocity.y -= 14 * GetFrameTime();
        obj->velocity.y = fminf(obj->velocity.y, 14);
    }else
    {
        obj->velocity.y = fmaxf(0, obj->velocity.y);
    }

    float rotationStrength = Lerp(0, time, speed/5);

    


    if(fabsf(rotationStrength) > time)
        rotationStrength = time * (rotationStrength / fabsf(rotationStrength));

    if(obj->grounded)
    {
        if(obj->airTime < 0)
            obj->airTime = 0;
    
        obj->airTime += GetFrameTime();
    } else
    {
        rotationStrength = time;
        
        if(obj->airTime > 0)
            obj->airTime = 0;

        obj->airTime -= GetFrameTime();
    }

    obj->wantedRotation = QuaternionNlerp(obj->wantedRotation, newRot, rotationStrength);
    
    if(obj->airTime > -0.2 && obj->boost <= -5)
    {
        obj->rotation = QuaternionLerp(obj->rotation, obj->wantedRotation, GetFrameTime()*9);
    }else if(obj->airTime > -0.2)
    {
        obj->rotation = QuaternionLerp(obj->rotation, obj->wantedRotation, GetFrameTime()*2);
    }
}

Texture2D LoadTextureWithFiltering(char * file)
{
    Texture2D tex = LoadTexture(file);
    GenTextureMipmaps(&tex);
    SetTextureFilter(tex, TEXTURE_FILTER_BILINEAR);
    return tex;
}

void main()
{
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(126, 126, "raylibkart");

    _shader = LoadShader("../assets/shader.vs", "../assets/shader.fs");
    int shaderPosUniform = GetShaderLocation(_shader, "pos");
    int shaderCameraUniform = GetShaderLocation(_shader, "camera");

    int shaderFarColorUniform = GetShaderLocation(_shader, "farColor");

    Color farColor = RED;

    Vector4 vec4 = (Vector4){0.7,0.7,0.7,1};

    SetShaderValue(_shader, shaderFarColorUniform, &vec4, SHADER_UNIFORM_VEC4);

    Model track = LoadModel("../assets/track2.obj");
    track.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTextureWithFiltering("../assets/road.png");
    track.materials[0].shader = _shader;
    track.materials[1].shader = _shader;
    track.materials[2].shader = _shader;

    track.materials[1].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTextureWithFiltering("../assets/leaves.png");
    track.materials[2].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTextureWithFiltering("../assets/woodtree.png");
    

    Model skybox = LoadModel("../assets/skybox.obj");
    skybox.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = LoadTextureWithFiltering("../assets/cubemap.png");

    Kart kart = LoadKart("../assets/kart.obj", (Vector3){0, 5, 0}, (Vector3){1, 1, 1});
    // kart.model.materials[0].shader = _shader;
    // kart.model.materials[1].shader = _shader;


    Camera camera = {0};
    camera.fovy = 80;
    camera.target = (Vector3){0, 0, 1};
    camera.up = (Vector3){0, 1, 0};
    camera.projection = CAMERA_PERSPECTIVE;

    SetTargetFPS(120);

    Vector3 checkpoint = {0,0,0};

    while(!WindowShouldClose())
    {

        Quaternion rot = QuaternionMultiply(kart.surfaceRotation, kart.rotation);
        Ray ray = {0};
        ray.position = Vector3Add(kart.position, Vector3RotateByQuaternion((Vector3){0, 0.5+kart.box.min.y, 0}, rot));
        ray.direction = Vector3RotateByQuaternion((Vector3){0, -1, 0}, rot);


        RayCollision collision = GetRayCollisionMesh(ray, track.meshes[0], track.transform);

        kart.grounded = false;


        if(collision.hit)
        {
            if(collision.distance <= 5) //snap to ground
            {
                Vector3 newNormal = collision.normal;
                if(collision.distance <= 0.5)
                {
                    Vector3 newPos = Vector3Add(kart.position, Vector3Scale(ray.direction, collision.distance-0.5));
                    kart.position = Vector3Lerp(kart.position, newPos, GetFrameTime()*15);
                    kart.grounded = true;

                    if(IsKeyDown(KEY_SPACE) && kart.boost <= -5 && kart.airTime < -0.8)
                    {
                        kart.velocity.z += 10;
                    }
                }

                Vector3 pos1 = collision.point;

                ray.position = Vector3Add(kart.position, Vector3RotateByQuaternion((Vector3){0, 0.5, 1.5}, rot));

                collision = GetRayCollisionMesh(ray, track.meshes[0], track.transform);

                // applyNormalToQuaternion(&kart.rotation, collision.normal.x, collision.normal.y, collision.normal.z);

                float strength = Vector3DotProduct(kart.normal, collision.normal);

                strength = pow(strength, 2);
                strength = 1;
                // strength = fminf(strength, )

                if(collision.hit && collision.distance < 5 && Vector3DotProduct(kart.normal, collision.normal) > 0.5)
                {
                    if(kart.grounded)
                    {
                        kart.normal = Vector3Lerp(kart.normal, newNormal, strength);
                    }

                    printf("strength %.2f\n", strength);

                    Vector3 pos2 = collision.point;
                    Vector3 diff = Vector3Normalize(Vector3Subtract(pos1, pos2));
                    Vector3 normal = kart.normal;
                    Quaternion rot = QuaternionFromMatrix(MatrixLookAt(pos1, pos2, normal));
                    

                    rot = QuaternionMultiply(kart.rotation, rot);

                    Quaternion rot2 = QuaternionFromAxisAngle((Vector3){0,1,0}, PI);
                    rot = QuaternionMultiply(rot, rot2);

                    kart.surfaceRotation = QuaternionSlerp(kart.surfaceRotation, rot, strength);

                }

            }
        }

        



        ray.position = Vector3Add(kart.position, Vector3RotateByQuaternion((Vector3){0, 0.5+kart.box.min.y, 0}, rot));
        ray.direction = Vector3RotateByQuaternion((Vector3){0, 0, 1}, rot);


        collision = GetRayCollisionMesh(ray, track.meshes[0], track.transform);

        if(collision.hit && !kart.stopped)
        {
            if(collision.distance <= kart.box.max.z+kart.velocity.z*GetFrameTime())
            {
                printf("frontal collision incoming!\n");
                kart.stopped = true;
                kart.position = Vector3Subtract(kart.position, Vector3Scale(ray.direction, collision.distance));

                kart.velocity = Vector3Scale(Vector3Reflect(kart.velocity, collision.normal), 0.1);

                // if(kart.velocity.z > 0)
                //     kart.velocity.z = kart.velocity.z * -0.2;
            }
        }else
        {
            kart.stopped = false;
        }

        BeginDrawing();
            ClearBackground(BLUE);

            Vector3 newVel = {0};
            newVel.z = (IsKeyDown(KEY_W)*1 + IsKeyDown(KEY_S)*-1) * 15;
            newVel.x = (IsKeyDown(KEY_A)*1 + IsKeyDown(KEY_D)*-1) * 2;

            float boostAddition = 0;

            if(kart.boost > -5)
                newVel.x *= 2;

            kart.velocity.x = Lerp(kart.velocity.x, 0, GetFrameTime()*2);


            if(IsKeyPressed(KEY_SPACE) && kart.airTime > -0.2)
            {
                if(kart.velocity.z > 4)
                    kart.boost = -1;
                
                kart.velocity.y += 2;
            }else if(IsKeyDown(KEY_SPACE) && kart.boost > -5)
            {
                boostAddition = fabsf(newVel.x);

                if(kart.airTime < -0.2)
                    boostAddition = 0;

                kart.boost += GetFrameTime()*boostAddition*fmax(fmin(kart.velocity.z/5, 1), 0);

                if(boostAddition < 0.5)
                    kart.boost = Lerp(kart.boost, 0, GetFrameTime()*(5/(kart.boost+5)));

                boostAddition = fmaxf(boostAddition, 0);

                kart.boost = fminf(kart.boost, 8);
            }else if(IsKeyReleased(KEY_SPACE) && kart.airTime > -0.2)
            {
                kart.velocity.z += powf(fmaxf(kart.boost, 0)/3, 2);
                kart.boost = -5;
            }else
            {
                kart.boost = -5;
            }

            

            if(kart.velocity.z < 0 && newVel.z < 0)
                newVel.z *= 0.5;

            
            if(kart.grounded)
                kart.velocity.z = Lerp(kart.velocity.z, newVel.z, GetFrameTime()*0.5);

            float rotStrength = 4;

            if(newVel.x == 0)
                rotStrength = 15;

            Color boostColor = ColorAlpha(ORANGE, 0);

            if(kart.boost)
            {
                boostColor = ORANGE;
                if(kart.boost > 2)
                    boostColor = RED;
                if(kart.boost > 6)
                    boostColor = BLUE;
            }
            
            if(!kart.grounded)
                newVel.x *= 0.7;


            if(kart.boost > -5)
                rotStrength *= 2;

            
            
            kart.rotVel.y = Lerp(kart.rotVel.y, newVel.x, GetFrameTime()*rotStrength);

            KartPhysics(&kart, GetFrameTime());

            SetShaderValue(_shader, shaderPosUniform, &kart.position,SHADER_UNIFORM_VEC3);
            SetShaderValue(_shader, shaderCameraUniform, &camera.position,SHADER_UNIFORM_VEC3);

            rot = QuaternionMultiply(kart.surfaceRotation, kart.wantedRotation);

            Vector3 newCameraPos = Vector3Add(kart.position, Vector3RotateByQuaternion((Vector3){0, 1.5,-3}, rot));
            camera.position = Vector3Lerp(camera.position, newCameraPos, 10*GetFrameTime());
            camera.target = kart.position;
            camera.up = Vector3Lerp(camera.up, kart.normal, GetFrameTime()*5);

            camera.fovy = Lerp(camera.fovy, 70 + fabsf(kart.velocity.z), GetFrameTime()*2);

            if(Vector3Distance(camera.position, kart.position) > 15)
                camera.position = newCameraPos;

            rot = QuaternionMultiply(kart.surfaceRotation, kart.rotation);

            BeginMode3D(camera);

                // rlDisableBackfaceCulling();
                rlDisableDepthMask();
                    DrawModel(skybox, camera.position, 1.0f, WHITE);
                // rlEnableBackfaceCulling();
                rlEnableDepthMask();

                DrawModel(track, (Vector3){0}, 1, WHITE);
                // DrawRay(ray, RED);
                kart.model.materials[2].maps[MATERIAL_MAP_DIFFUSE].color = ColorAlpha(boostColor, sqrtf(kart.boost/4)*(boostAddition/6));
                // DrawSphere(Vector3Subtract(kart.position, Vector3RotateByQuaternion((Vector3){0, 0, 1}, rot)), sqrtf(kart.boost/4)/4, );
                DrawKart(kart);
            EndMode3D();

            char str[100];
            snprintf(str, 100, "%.0f", kart.velocity.z*5);

            Color speedTextColor = WHITE;

            if(kart.velocity.z > 10)
            {
                speedTextColor = ORANGE;

                if(kart.velocity.z > 15)
                    speedTextColor = RED;

                if(kart.velocity.z > 20)
                    speedTextColor = BLUE;
            }
            DrawText(str, GetRenderWidth()*0.8, GetRenderHeight()*0.8, GetRenderWidth()*0.07, speedTextColor);
            
            DrawFPS(1,1);
        EndDrawing();
    }
}