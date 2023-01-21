/*
    James William Fletcher (github.com/mrbid)
        July 2022 - December 2022

    https://SeaPusher.com & https://pusha.one

    A highly optimised and efficient coin pusher
    implementation.

    This was not made in a way that is easily extensible,
    the code can actually seem kind of confusing to
    initially understand.

    The only niggle is the inability to remove depth buffering
    when rendering the game over text due to the way I exported
    the mesh, disabling depth results in the text layers inverting
    causing the shadow to occlude the text face. It's not the
    end of the world, but could be fixed.
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define uint GLushort // it's a short don't forget that
#define sint GLshort  // and this.
#define f32 GLfloat
#define forceinline __attribute__((always_inline)) inline

#include "inc/gl.h"
#define GLFW_INCLUDE_NONE
#include "inc/glfw3.h"

#ifndef __x86_64__
    #define NOSSE
#endif

#define SEIR_RAND

#include "esAux2.h"
#include "inc/res.h"

#include "assets/scene.h"
#include "assets/coin.h"
#include "assets/diver.h"
#include "assets/merman.h"
#include "assets/skeleton.h"
#include "assets/gameover.h"

//*************************************
// globals
//*************************************
GLFWwindow* window;
uint winw = 1024, winh = 768;
double t = 0;   // time
f32 dt = 0;     // delta time
double fc = 0;  // frame count
double lfct = 0;// last frame count time
f32 aspect;
double x,y,lx,ly;
double rww, ww, rwh, wh, ww2, wh2;
double uw, uh, uw2, uh2; // normalised pixel dpi
double maxfps = 144.0;
double touch_margin = 120.0;
double mx=0, my=0;
uint md=0, ortho=0;

// render state id's
GLint projection_id;
GLint modelview_id;
GLint position_id;
GLint lightpos_id;
GLint solidcolor_id;
GLint color_id;
GLint opacity_id;
GLint normal_id;

// render state inputs
vec lightpos = {0.f, 10.f, 13.f};

// render state matrices
mat projection;
mat view;
mat model;
mat modelview;

// models
ESModel mdlScene;
ESModel mdlCoin;
ESModel mdlDiver;
ESModel mdlMerman;
ESModel mdlSkeleton;
ESModel mdlGameover;

// game vars
f32 gold_stack = 64.f;  // line 740+ defining these as float32 eliminates the need to cast in mTranslate()
f32 silver_stack = 64.f;// function due to the use of a float32 also in the for(f32 i;) loop.
uint active_coin = 0;
uint inmotion = 0;
double gameover = 0;
uint isnewcoin = 0;
#define PUSH_SPEED 1.6f

typedef struct
{
    f32 x, y, r;
    signed char color;
} coin; // 4+4+4+1 = 13 bytes, 3 bytes padding to 16 byte cache line
#define MAX_COINS 130
coin coins[MAX_COINS] = {0};

uint trophies[6] = {0};

//*************************************
// game functions
//*************************************
void timestamp(char* ts)
{
    const time_t tt = time(0);
    strftime(ts, 16, "%H:%M:%S", localtime(&tt));
}

void setActiveCoin(const uint color)
{
    for(int i=0; i < MAX_COINS; i++)
    {
        if(coins[i].color == -1)
        {
            coins[i].color = color;
            active_coin = i;
            return;
        }
    }
}

void takeStack()
{
    if(gameover == 0)
    {
        if(silver_stack != 0.f)
        {
            // play a silver coin
            isnewcoin = 1;
            setActiveCoin(0);
            inmotion = 1;
        }
        else if(gold_stack != 0.f)
        {
            // play a gold coin
            isnewcoin = 2;
            setActiveCoin(1);
            inmotion = 1;
        }

        if(inmotion == 1)
        {
            if(mx < touch_margin)
            {
                coins[active_coin].x = -1.90433f;
                coins[active_coin].y = -4.54055f;
            }
            else if(mx > ww-touch_margin)
            {
                coins[active_coin].x = 1.90433f;
                coins[active_coin].y = -4.54055f;
            }
            else
            {
                coins[active_coin].x = -1.90433f+(((mx-touch_margin)*rww)*3.80866f);
                coins[active_coin].y = -4.54055f;
            }
        }
    }
}

void injectFigure()
{
    if(inmotion != 0)
        return;
    
    int fc = -1;
    for(int i=0; i < 3; i++)
    {
        if(coins[i].color == -1)
        {
            active_coin = i;
            fc = i;
            coins[i].color = 1;
            break;
        }
    }

    if(fc != -1)
    {
        coins[active_coin].x = esRandFloat(-1.90433f, 1.90433f);
        coins[active_coin].y = -4.54055f;
        inmotion = 1;
    }
}

int insidePitch(const f32 x, const f32 y, const f32 r)
{
    // off bottom
    if(y < -4.03414f+r)
        return 0;
    
    // first left & right
    if(y < -2.22855f)
    {
        if(x < (-2.22482f - (0.77267f*(fabsf(y+4.03414f) * 0.553835588f))) + r)
            return 0;
        else if(x > (2.22482f + (0.77267f*(fabsf(y+4.03414f) * 0.553835588f))) - r)
            return 0;
    }
    else if(y < -0.292027f) // second left & right
    {
        if(x < (-2.99749f - (0.41114f*(fabsf(y+2.22855f) * 0.516389426f))) + r)
            return 0;
        else if(x > (2.99749f + (0.41114f*(fabsf(y+2.22855f) * 0.516389426f))) - r)
            return 0;
    }
    else if(y < 1.45439f) // third left & right
    {
        if(x < -3.40863f + r)
            return 0;
        else if(x > 3.40863f - r)
            return 0;
    }

    return 1;
}

int collision(int ci)
{
    for(int i=0; i < MAX_COINS; i++)
    {
        if(i == ci || coins[i].color == -1){continue;}
        const f32 xm = (coins[i].x - coins[ci].x);
        const f32 ym = (coins[i].y - coins[ci].y);
        const f32 radd = coins[i].r+coins[ci].r;
        if(xm*xm + ym*ym < radd*radd)
            return 1;
    }
    return 0;
}

uint stepCollisions()
{
    uint was_collision = 0;
    for(int i=0; i < MAX_COINS; i++)
    {
        if(coins[i].color == -1){continue;}
        for(int j=0; j < MAX_COINS; j++)
        {
            if(i == j || coins[j].color == -1 || j == active_coin){continue;}
            f32 xm = (coins[i].x - coins[j].x);
            xm += esRandFloat(-0.01f, 0.01f); // add some random offset to our unit vector, very subtle but works so well!
            const f32 ym = (coins[i].y - coins[j].y);
            f32 d = xm*xm + ym*ym;
            const f32 cr = coins[i].r+coins[j].r;
            if(d < cr*cr)
            {
                d = sqrtps(d);
                const f32 len = 1.f/d;
                const f32 uy = (ym * len);
                if(uy > 0.f){continue;} // best hack ever to massively simplify
                const f32 m = d-cr;
                coins[j].x += (xm * len) * m;
                coins[j].y += uy * m;

                // first left & right
                if(coins[j].y < -2.22855f)
                {
                    const f32 fl = (-2.22482f - (0.77267f*(fabsf(coins[j].y+4.03414f) * 0.553835588f))) + coins[j].r;
                    if(coins[j].x < fl)
                    {
                        coins[j].x = fl;
                    }
                    else
                    {
                        const f32 fr = ( 2.22482f + (0.77267f*(fabsf(coins[j].y+4.03414f) * 0.553835588f))) - coins[j].r;
                        if(coins[j].x > fr)
                            coins[j].x = fr;
                    }
                }
                else if(coins[j].y < -0.292027f) // second left & right
                {
                    const f32 fl = (-2.99749f - (0.41114f*(fabsf(coins[j].y+2.22855f) * 0.516389426f))) + coins[j].r;
                    if(coins[j].x < fl)
                    {
                        coins[j].x = fl;
                    }
                    else
                    {
                        const f32 fr = (2.99749f + (0.41114f*(fabsf(coins[j].y+2.22855f) * 0.516389426f))) - coins[j].r;
                        if(coins[j].x > fr)
                            coins[j].x = fr;
                    }
                }
                else if(coins[j].y < 1.45439f) // third left & right
                {
                    const f32 fl = -3.40863f + coins[j].r;
                    if(coins[j].x < fl)
                    {
                        coins[j].x = fl;
                    }
                    else
                    {
                        const f32 fr = 3.40863f - coins[j].r;
                        if(coins[j].x > fr)
                            coins[j].x = fr;
                    }
                }
                else if(coins[j].y < 2.58397f) // first house goal
                {
                    const f32 fl = (-3.40863f + (0.41113f*(fabsf(coins[j].y-1.45439f) * 0.885284796f)));
                    if(coins[j].x < fl)
                    {
                        coins[j].color = -1;
                    }
                    else
                    {
                        const f32 fr = (3.40863f - (0.41113f*(fabsf(coins[j].y-1.45439f) * 0.885284796f)));
                        if(coins[j].x > fr)
                            coins[j].color = -1;
                    }
                }
                else if(coins[j].y < 3.70642f) // second house goal
                {
                    const f32 fl = (-2.9975f + (1.34581f*(fabsf(coins[j].y-2.58397f) * 0.890908281f)));
                    if(coins[j].x < fl)
                    {
                        coins[j].color = -1;
                    }
                    else
                    {
                        const f32 fr = (2.9975f - (1.34581f*(fabsf(coins[j].y-2.58397f) * 0.890908281f)));
                        if(coins[j].x > fr)
                            coins[j].color = -1;
                    }
                }
                else if(coins[j].y < 4.10583f) // silver goal
                {
                    const f32 fl = (-1.65169f + (1.067374f*(fabsf(coins[j].y-3.70642f) * 2.503692947f)));
                    if(coins[j].x < fl)
                    {
                        if(j == 0)
                        {
                            if(trophies[0] == 1) // already have? then reward coins!
                                silver_stack += 6.f;
                            else
                                trophies[0] = 1;
                        }
                        else if(j == 1)
                        {
                            if(trophies[1] == 1) // already have? then reward coins!
                                silver_stack += 6.f;
                            else
                                trophies[1] = 1;
                        }
                        else if(j == 2)
                        {
                            if(trophies[2] == 1) // already have? then reward coins!
                                silver_stack += 6.f;
                            else
                                trophies[2] = 1;
                        }
                        else
                        {
                            if(coins[j].color == 0)
                                silver_stack += 1.f;
                            else if(coins[j].color == 1)
                                silver_stack += 2.f;
                        }

                        coins[j].color = -1;
                    }
                    else
                    {
                        const f32 fr = (1.65169f - (1.067374f*(fabsf(coins[j].y-3.70642f) * 2.503692947f)));
                        if(coins[j].x > fr)
                        {
                            if(j == 0)
                            {
                                if(trophies[0] == 1) // already have? then reward coins!
                                    silver_stack += 6.f;
                                else
                                    trophies[0] = 1;
                            }
                            else if(j == 1)
                            {
                                if(trophies[1] == 1) // already have? then reward coins!
                                    silver_stack += 6.f;
                                else
                                    trophies[1] = 1;
                            }
                            else if(j == 2)
                            {
                                if(trophies[2] == 1) // already have? then reward coins!
                                    silver_stack += 6.f;
                                else
                                    trophies[2] = 1;
                            }
                            else
                            {
                                if(coins[j].color == 0)
                                    silver_stack += 1.f;
                                else if(coins[j].color == 1)
                                    silver_stack += 2.f;
                            }

                            coins[j].color = -1;
                        }
                    }
                }
                else if(coins[j].y >= 4.31457f) // gold goal
                {
                    if(coins[j].x >= -0.584316f && coins[j].x <= 0.584316f)
                    {
                        if(j == 0)
                        {
                            if(trophies[3] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[3] = 1;
                        }
                        else if(j == 1)
                        {
                            if(trophies[4] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[4] = 1;
                        }
                        else if(j == 2)
                        {
                            if(trophies[5] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[5] = 1;
                        }
                        else
                        {
                            if(coins[j].color == 0)
                                gold_stack += 1.f;
                            else if(coins[j].color == 1)
                                gold_stack += 2.f;
                        }

                        coins[j].color = -1;
                    }
                    else
                    {
                        coins[j].color = -1;

                        if(j == 0)
                        {
                            if(trophies[3] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[3] = 1;
                        }
                        else if(j == 1)
                        {
                            if(trophies[4] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[4] = 1;
                        }
                        else if(j == 2)
                        {
                            if(trophies[5] == 1) // already have? then reward coins!
                                gold_stack += 6.f;
                            else
                                trophies[5] = 1;
                        }
                        else
                            silver_stack += 1.f;
                    }
                }
                
                was_collision++;
            }
        }
    }
    return was_collision;
}

void newGame()
{
    // seed randoms
    srand(time(0));

    // defaults
    gold_stack = 64.f;
    silver_stack = 64.f;
    active_coin = 0;
    inmotion = 0;
    gameover = 0;
    trophies[0] = 0;
    trophies[1] = 0;
    trophies[2] = 0;
    trophies[3] = 0;
    trophies[4] = 0;
    trophies[5] = 0;
    for(int i=0; i < MAX_COINS; i++)
    {
        coins[i].color = -1;
        coins[i].r = 0.3f;
    }

    // trophies
    for(int i=0; i < 3; i++)
    {
        coins[i].color = 0;
        coins[i].r = 0.34f;

        coins[i].x = esRandFloat(-3.40863f, 3.40863f);
        coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
        while(insidePitch(coins[i].x, coins[i].y, coins[i].r) == 0 || collision(i) == 1)
        {
            coins[i].x = esRandFloat(-3.40863f, 3.40863f);
            coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
        }
    }

    // coins
    const double lt = glfwGetTime();
    for(int i=3; i < MAX_COINS; i++)
    {
        coins[i].x = esRandFloat(-3.40863f, 3.40863f);
        coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
        uint tl = 0;
        while(insidePitch(coins[i].x, coins[i].y, coins[i].r) == 0 || collision(i) == 1)
        {
            coins[i].x = esRandFloat(-3.40863f, 3.40863f);
            coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
            if(glfwGetTime()-lt > 0.033){tl=1;break;} // 33ms timeout
        }
        if(tl==1){break;}
        coins[i].color = esRand(0, 4);
        if(coins[i].color > 1){coins[i].color = 0;}
    }

    // const int mc2 = MAX_COINS/2;
    // for(int i=3; i < mc2; i++)
    // {
    //     coins[i].x = esRandFloat(-3.40863f, 3.40863f);
    //     coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
    //     while(insidePitch(coins[i].x, coins[i].y, coins[i].r) == 0 || collision(i) == 1)
    //     {
    //         coins[i].x = esRandFloat(-3.40863f, 3.40863f);
    //         coins[i].y = esRandFloat(-4.03414f, 1.45439f-coins[i].r);
    //     }
    //     coins[i].color = esRand(0, 4);
    //     if(coins[i].color > 1){coins[i].color = 0;}
    // }
}

//*************************************
// render functions
//*************************************
forceinline void modelBind1(const ESModel* mdl)
{
    glBindBuffer(GL_ARRAY_BUFFER, mdl->vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);

    glBindBuffer(GL_ARRAY_BUFFER, mdl->nid);
    glVertexAttribPointer(normal_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(normal_id);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdl->iid);
}

forceinline void modelBind3(const ESModel* mdl)
{
    glBindBuffer(GL_ARRAY_BUFFER, mdl->cid);
    glVertexAttribPointer(color_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(color_id);

    glBindBuffer(GL_ARRAY_BUFFER, mdl->vid);
    glVertexAttribPointer(position_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(position_id);

    glBindBuffer(GL_ARRAY_BUFFER, mdl->nid);
    glVertexAttribPointer(normal_id, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(normal_id);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mdl->iid);
}

//*************************************
// update & render
//*************************************
//*************************************
// update & render
//*************************************
void main_loop(uint dotick)
{   
//*************************************
// begin render
//*************************************
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

//*************************************
// main render
//*************************************

    // mouse pos
    glfwGetCursorPos(window, &mx, &my);

    // camera
    mIdent(&view);
    mTranslate(&view, 0.f, 0.f, -13.f);
    if(ortho == 1)
        mRotY(&view, 50.f*DEG2RAD);
    else
        mRotY(&view, 62.f*DEG2RAD);

    // inject a new figure if time has come
    injectFigure();
    
    // prep scene for rendering
    shadeLambert3(&position_id, &projection_id, &modelview_id, &lightpos_id, &normal_id, &color_id, &opacity_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (f32*) &projection.m[0][0]);
    glUniform3f(lightpos_id, lightpos.x, lightpos.y, lightpos.z);
    glUniform1f(opacity_id, 1.0f);

    // render scene
    glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &view.m[0][0]);
    modelBind3(&mdlScene);
    glDrawElements(GL_TRIANGLES, scene_numind, GL_UNSIGNED_SHORT, 0);

    // detect gameover
    if(gold_stack < 0.f){gold_stack = 0.f;}
    if(silver_stack < 0.f){silver_stack = 0.f;}
    if(gameover > 0 && (gold_stack != 0.f || silver_stack != 0.f))
    {
        gameover = 0;
    }
    else if(gold_stack == 0.f && silver_stack == 0.f)
    {
        if(gameover == 0)
            gameover = t+3.0;
    }

    // render game over
    if(gameover > 0 && t > gameover)
    {
        modelBind3(&mdlGameover);
        glDrawElements(GL_TRIANGLES, gameover_numind, GL_UNSIGNED_SHORT, 0);
    }

    // prep pieces for rendering
    shadeLambert1(&position_id, &projection_id, &modelview_id, &lightpos_id, &normal_id, &color_id, &opacity_id);
    glUniformMatrix4fv(projection_id, 1, GL_FALSE, (f32*) &projection.m[0][0]);
    glUniform3f(lightpos_id, lightpos.x, lightpos.y, lightpos.z);
    glUniform1f(opacity_id, 1.0f);

    // coin
    modelBind1(&mdlCoin);

    // targeting coin
    if(gold_stack > 0.f || silver_stack > 0.f)
    {
        if(coins[active_coin].color == 1)
            glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);
        else
            glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
        if(md == 1)
        {
            if(mx < touch_margin)
            {
                coins[active_coin].x = -1.90433f;
                coins[active_coin].y = -4.54055f;
            }
            else if(mx > ww-touch_margin)
            {
                coins[active_coin].x = 1.90433f;
                coins[active_coin].y = -4.54055f;
            }
            else
            {
                coins[active_coin].x = -1.90433f+(((mx-touch_margin)*rww)*3.80866f);
                coins[active_coin].y = -4.54055f;
            }
        }
        else if(inmotion == 0)
        {
            if(silver_stack > 0.f)
                glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
            else
                glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);

            mIdent(&model);

            if(mx < touch_margin)
                mTranslate(&model, -1.90433f, -4.54055f, 0);
            else if(mx > ww-touch_margin)
                mTranslate(&model, 1.90433f, -4.54055f, 0);
            else
                mTranslate(&model, -1.90433f+(((mx-touch_margin)*rww)*3.80866f), -4.54055f, 0);

            mMul(&modelview, &model, &view);
            glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
            glDrawElements(GL_TRIANGLES, coin_numind, GL_UNSIGNED_SHORT, 0);
        }
    }

    // do motion
    if(inmotion == 1)
    {
        if(coins[active_coin].y < -3.73414f)
        {
            coins[active_coin].y += PUSH_SPEED * dt;
            stepCollisions();
        }
        else
        {
            inmotion = 0;

            if(isnewcoin > 0)
            {
                if(isnewcoin == 1)
                    silver_stack -= 1.f;
                else
                    gold_stack -= 1.f;

                isnewcoin = 0;
            }
        }
    }

    // gold stack
    glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);
    f32 gss = gold_stack;
    if(silver_stack == 0.f){gss -= 1.f;}
    if(gss < 0.f){gss = 0.f;}
    for(f32 i = 0.f; i < gss; i += 1.f)
    {
        mIdent(&model);
        if(ortho == 0)
            mTranslate(&model, -2.62939f, -4.54055f, 0.0406f*i);
        else
            mTranslate(&model, -4.62939f, -4.54055f, 0.0406f*i);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, coin_numind, GL_UNSIGNED_SHORT, 0);
    }

    // silver stack
    glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
    f32 sss = silver_stack-1.f;
    if(sss < 0.f){sss = 0.f;}
    for(f32 i = 0.f; i < sss; i += 1.f)
    {
        mIdent(&model);
        if(ortho == 0)
            mTranslate(&model, 2.62939f, -4.54055f, 0.0406f*i);
        else
            mTranslate(&model, 4.62939f, -4.54055f, 0.0406f*i);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, coin_numind, GL_UNSIGNED_SHORT, 0);
    }

    // pitch coins
    for(int i=3; i < MAX_COINS; i++)
    {
        if(coins[i].color == -1)
            continue;
        
        if(coins[i].color == 0)
            glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
        else
            glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);

        mIdent(&model);
        mTranslate(&model, coins[i].x, coins[i].y, 0.f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, coin_numind, GL_UNSIGNED_SHORT, 0);
    }

    // trophies
    glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);

    if(coins[0].color != -1)
    {
        modelBind1(&mdlDiver);
        mIdent(&model);
        mTranslate(&model, coins[0].x, coins[0].y, 0.f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, diver_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(coins[1].color != -1)
    {
        modelBind1(&mdlMerman);
        mIdent(&model);
        mTranslate(&model, coins[1].x, coins[1].y, 0.f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, merman_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(coins[2].color != -1)
    {
        modelBind1(&mdlSkeleton);
        mIdent(&model);
        mTranslate(&model, coins[2].x, coins[2].y, 0.f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, skeleton_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[0] == 1)
    {
        glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
        modelBind1(&mdlDiver);
        mIdent(&model);
        mTranslate(&model, 3.92732f, 1.0346f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, diver_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[1] == 1)
    {
        glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
        modelBind1(&mdlMerman);
        mIdent(&model);
        mTranslate(&model, 3.65552f, -1.30202f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, merman_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[2] == 1)
    {
        glUniform3f(color_id, 0.68235f, 0.70196f, 0.72941f);
        modelBind1(&mdlSkeleton);
        mIdent(&model);
        mTranslate(&model, 3.01911f, -3.23534f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, skeleton_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[3] == 1)
    {
        glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);
        modelBind1(&mdlDiver);
        mIdent(&model);
        mTranslate(&model, -3.92732f, 1.0346f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, diver_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[4] == 1)
    {
        glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);
        modelBind1(&mdlMerman);
        mIdent(&model);
        mTranslate(&model, -3.65552f, -1.30202f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, merman_numind, GL_UNSIGNED_SHORT, 0);
    }

    if(trophies[5] == 1)
    {
        glUniform3f(color_id, 0.76471f, 0.63529f, 0.18824f);
        modelBind1(&mdlSkeleton);
        mIdent(&model);
        mTranslate(&model, -3.01911f, -3.23534f, 0.f);
        mRotZ(&model, t*0.1f);
        mMul(&modelview, &model, &view);
        glUniformMatrix4fv(modelview_id, 1, GL_FALSE, (f32*) &modelview.m[0][0]);
        glDrawElements(GL_TRIANGLES, skeleton_numind, GL_UNSIGNED_SHORT, 0);
    }

//*************************************
// swap buffers / display render
//*************************************
    glfwSwapBuffers(window);
}

//*************************************
// Input Handelling
//*************************************
void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(inmotion == 0 && button == GLFW_MOUSE_BUTTON_LEFT)
        {
            if(gameover > 0)
            {
                if(glfwGetTime() > gameover+3.0)
                    newGame();
                return;
            }
            takeStack();
            md = 1;
        }
        else if(button == GLFW_MOUSE_BUTTON_RIGHT)
        {
            static uint cs = 1;
            cs = 1 - cs;
            if(cs == 0)
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
            else
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        }
    }
    else if(action == GLFW_RELEASE)
    {
        if(button == GLFW_MOUSE_BUTTON_LEFT)
        {
            inmotion = 1;
            md = 0;
        }
    }
}

void window_size_callback(GLFWwindow* window, int width, int height)
{
    winw = width;
    winh = height;

    glViewport(0, 0, winw, winh);
    aspect = (f32)winw / (f32)winh;
    ww = winw;
    wh = winh;
    if(ortho == 1){touch_margin = ww*0.3076923192f;}
    else{touch_margin = ww*0.2058590651f;}
    rww = 1.0/(ww-touch_margin*2.0);
    rwh = 1.0/wh;
    ww2 = ww/2.0;
    wh2 = wh/2.0;
    uw = (double)aspect / ww;
    uh = 1.0/wh;
    uw2 = (double)aspect / ww2;
    uh2 = 1.0/wh2;

    mIdent(&projection);

    if(ortho == 1)
        mOrtho(&projection, -5.0f, 5.0f, -3.2f, 3.4f, 0.01f, 320.f);
    else
    {
        if(winw > winh)
            aspect = (f32)winw / (f32)winh;
        else
            aspect = (f32)winh / (f32)winw;
        mPerspective(&projection, 30.0f, aspect, 0.01f, 320.f);
    }
}

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(action == GLFW_PRESS)
    {
        if(key == GLFW_KEY_F)
        {
            if(t-lfct > 2.0)
            {
                char strts[16];
                timestamp(&strts[0]);
                const double nfps = fc/(t-lfct);
                printf("[%s] FPS: %g\n", strts, nfps);
                maxfps = nfps;
                dt = 1.0f / (float)maxfps;
                lfct = t;
                fc = 0;
            }
        }
        else if(key == GLFW_KEY_C)
        {
            ortho = 1 - ortho;
            window_size_callback(window, winw, winh);
        }
    }
}

//*************************************
// Process Entry Point
//*************************************
int main(int argc, char** argv)
{
    // allow custom msaa level
    int msaa = 16;
    if(argc >= 2){msaa = atoi(argv[1]);}

    // allow framerate cap
    if(argc >= 3){maxfps = atof(argv[2]);}

    // help
    printf("----\n");
    printf("SeaPusher.com\n");
    printf("----\n");
    printf("James William Fletcher (github.com/mrbid)\n");
    printf("----\n");
    printf("Argv(2): msaa, maxfps\n");
    printf("e.g; ./uc 16 60\n");
    printf("----\n");
    printf("Left Click = Release coin\n");
    printf("Right Click = Show/hide cursor\n");
    printf("C = Orthographic/Perspective\n");
    printf("F = FPS to console\n");
    printf("----\n");
    printf("Figurine 3D assets by BigMrTong:\n");
    printf("https://www.thingiverse.com/bigmrtong\n");
    printf("----\n");
    printf("Font (sortof) by Celtic Sea by Art Designs by Sue\n");
    printf("https://www.fontspace.com/celtic-sea-font-f58168\n");
    printf("\"I found this great font on a title page of an old book called Songs of the Hebrides. I named it Celtic Sea because of the large number of songs about the sea, sea creatures, and mythical sea beings, such as kelpies and mermaids.\"\n");
    printf("----\n");
    printf("Rules:\n");
    printf("Getting a gold coin in a silver slot rewards you 2x silver coins.\n");
    printf("Getting a gold coin in a gold slot rewards you 2x gold coins.\n");
    printf("Getting a figurine in a gold slot when you already have the gold figurine gives you 6x gold coins.\n");
    printf("Getting a figurine in a silver slot when you already have the silver figurine gives you 6x silver coins.\n");
    printf("----\n");

    // init glfw
    if(!glfwInit()){printf("glfwInit() failed.\n"); exit(EXIT_FAILURE);}
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SAMPLES, msaa);
    window = glfwCreateWindow(winw, winh, "Detecting frame rate...", NULL, NULL);
    if(!window)
    {
        printf("glfwCreateWindow() failed.\n");
        glfwTerminate();
        exit(EXIT_FAILURE);
    }
    const GLFWvidmode* desktop = glfwGetVideoMode(glfwGetPrimaryMonitor());
    glfwSetWindowPos(window, (desktop->width/2)-(winw/2), (desktop->height/2)-(winh/2)); // center window on desktop
    glfwSetWindowSizeCallback(window, window_size_callback);
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);
    glfwSwapInterval(0); // 0 for immediate updates, 1 for updates synchronized with the vertical retrace, -1 for adaptive vsync

    // set icon
    glfwSetWindowIcon(window, 1, &(GLFWimage){16, 16, (unsigned char*)&icon_image.pixel_data});

//*************************************
// bind vertex and index buffers
//*************************************

    // ***** BIND SCENE *****
    esBind(GL_ARRAY_BUFFER, &mdlScene.cid, scene_colors, sizeof(scene_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlScene.vid, scene_vertices, sizeof(scene_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlScene.nid, scene_normals, sizeof(scene_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlScene.iid, scene_indices, sizeof(scene_indices), GL_STATIC_DRAW);

    // ***** BIND GAMEOVER *****
    esBind(GL_ARRAY_BUFFER, &mdlGameover.cid, gameover_colors, sizeof(gameover_colors), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlGameover.vid, gameover_vertices, sizeof(gameover_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlGameover.nid, gameover_normals, sizeof(gameover_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlGameover.iid, gameover_indices, sizeof(gameover_indices), GL_STATIC_DRAW);

    // ***** BIND COIN *****
    esBind(GL_ARRAY_BUFFER, &mdlCoin.vid, coin_vertices, sizeof(coin_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlCoin.nid, coin_normals, sizeof(coin_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlCoin.iid, coin_indices, sizeof(coin_indices), GL_STATIC_DRAW);

    // ***** BIND DIVER *****
    esBind(GL_ARRAY_BUFFER, &mdlDiver.vid, diver_vertices, sizeof(diver_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlDiver.nid, diver_normals, sizeof(diver_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlDiver.iid, diver_indices, sizeof(diver_indices), GL_STATIC_DRAW);

    // ***** BIND MERMAN *****
    esBind(GL_ARRAY_BUFFER, &mdlMerman.vid, merman_vertices, sizeof(merman_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlMerman.nid, merman_normals, sizeof(merman_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlMerman.iid, merman_indices, sizeof(merman_indices), GL_STATIC_DRAW);

    // ***** BIND SKELETON *****
    esBind(GL_ARRAY_BUFFER, &mdlSkeleton.vid, skeleton_vertices, sizeof(skeleton_vertices), GL_STATIC_DRAW);
    esBind(GL_ARRAY_BUFFER, &mdlSkeleton.nid, skeleton_normals, sizeof(skeleton_normals), GL_STATIC_DRAW);
    esBind(GL_ELEMENT_ARRAY_BUFFER, &mdlSkeleton.iid, skeleton_indices, sizeof(skeleton_indices), GL_STATIC_DRAW);

//*************************************
// compile & link shader programs
//*************************************

    makeLambert1();
    makeLambert3();

//*************************************
// configure render options
//*************************************

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.52941f, 0.80784f, 0.92157f, 0.0f);

//*************************************
// execute update / render loop
//*************************************

    // new game
    window_size_callback(window, winw, winh);
    newGame();
    
    // init
    t = glfwGetTime();
    lfct = t;
    dt = 1.0f / (float)maxfps; // fixed timestep delta-time

    // detect frame rate
    time_t ac = time(0) + 1;
    uint fct = 0;
    
    // fps accurate event loop
    useconds_t wait_interval = 1000000 / maxfps; // fixed timestep
    if(wait_interval == 0){wait_interval = 100;} // limited to 10,000 FPS maximum
    useconds_t wait = wait_interval;
    while(!glfwWindowShouldClose(window))
    {
        usleep(wait);
        t = glfwGetTime();

        // auto correct max fps
        if(time(0) > ac)
        {
            const double nfps = fc/(t-lfct);
            if(fabs(nfps - maxfps) > 6.f)
            {
                char strts[16];
                timestamp(&strts[0]);
                printf("[%s] maxfps auto corrected from %.2f to %.2f.\n", strts, maxfps, nfps);
                char titlestr[256];
                sprintf(titlestr, "FPS: %.2f", nfps);
                glfwSetWindowTitle(window, titlestr);
            }
            maxfps = nfps;
            char titlestr[256];
            sprintf(titlestr, "SeaPusher.com - FPS: %.0f", nfps);
            glfwSetWindowTitle(window, titlestr);
            dt = 1.0f / (float)maxfps;
            ac = time(0) + 6;
            fct = 1;
        }
        
        // don't tick our internal state until we know we have a decent delta-time [dt]
        glfwPollEvents();
        main_loop(fct); // I don't actually use fct in this but it can be important.

        // accurate fps
        wait = wait_interval - (useconds_t)((glfwGetTime() - t) * 1000000.0);
        if(wait > wait_interval){wait = wait_interval;}
        //printf("%u: %u - %u\n", wait_interval, wait, (useconds_t)((glfwGetTime() - t) * 1000000.0));
        fc++;
    }

    // done
    glfwDestroyWindow(window);
    glfwTerminate();
    exit(EXIT_SUCCESS);
    return 0;
}
