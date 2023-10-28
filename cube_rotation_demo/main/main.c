#include <stdio.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "freertos/task.h"
#include "freertos/FreeRTOS.h"
#include "sh1106.h"
#include <math.h>

//i2c GPIO pin numbers
#define I2C_SDA_PIN_NUM 10
#define I2C_SCL_PIN_NUM 8

//i2c clock speed
#define I2C_CLK_SPEED 1000000 //1.0 Mhz

//sleep
#define SLEEP(ms) vTaskDelay(pdMS_TO_TICKS(ms))

//Vector
typedef struct Vector{
    double x, y, z;
}Vector;

typedef struct Transform{
    Vector translation;
    double rot_x, rot_y, rot_z, scale;
}Transform;

//Triangle
typedef struct Triangle{
    Vector v1, v2, v3;
}Triangle;

//Model
typedef struct Model{
    Triangle* tris;
    int num_tris;
    Transform transform;
}Model;

typedef enum Axis{
    X_AXIS,
    Y_AXIS,
    Z_AXIS
}Axis;

//Magnification Constant
#define MAGNIFY 50

//PI
#define PI 3.14159

esp_err_t i2c_master_init();

Vector vector_add(Vector v1, Vector v2);
Vector vector_scale(Vector v1, int scalar);

Vector build_vector(double x, double y, double z);
Vector transform_vector(Vector v, Transform transform);

Triangle build_triangle(Vector v1, Vector v2, Vector v3);
Triangle transform_triangle(Triangle tri, Transform transform);

Model* build_model(Triangle* tris, int num_tris, Transform trans);
Model* build_cube(Transform trans);

void render_model(Model* model, SH1106_t* dev); 


void draw_triangle(SH1106_t* dev, Triangle tri); 


Vector project_vector(Vector p);

void app_main(void)
{
    i2c_master_init(); 
    SH1106_t dev;
    sh1106_init(&dev);
    sh1106_clear_buffer(&dev);

    Transform trans = {
        .rot_x = 0,
        .rot_y = 0,
        .rot_z = 0,
        .scale = 50,
        .translation = build_vector(0, 0, 200)
    };

    Model* cube = build_cube(trans);

    double rads_per_cycle = 0.1;
    while(1){
        cube->transform.rot_y += rads_per_cycle;
        render_model(cube, &dev);
        sh1106_update_display(&dev);
        sh1106_clear_buffer(&dev);
        trans.rot_y += rads_per_cycle;
        SLEEP(50);
    }
}

Model* build_cube(Transform trans){
    Triangle* tris = malloc(sizeof(Triangle) * 12);

    Model* cube = build_model(tris, 12, trans);

    Vector v1, v2, v3;

    v1 = build_vector(1, 1, 1);
    v2 = build_vector(1, -1, 1);
    v3 = build_vector(-1, -1, 1);
    cube->tris[0] = build_triangle(v1, v2, v3); 

    v2 = build_vector(1, 1, 1);
    v1 = build_vector(-1, 1, 1);
    v3 = build_vector(-1, -1, 1);
    cube->tris[1] = build_triangle(v1, v2, v3); 

    v1 = build_vector(1, 1, 1);
    v2 = build_vector(1, 1, -1);
    v3 = build_vector(1, -1, 1);
    cube->tris[2] = build_triangle(v1, v2, v3); 

    v1 = build_vector(1, -1, 1);
    v3 = build_vector(1, -1, -1);
    v2 = build_vector(1, 1, -1);
    cube->tris[3] = build_triangle(v1, v2, v3);

    v1 = build_vector(-1, 1, 1);
    v3 = build_vector(-1, 1, -1);
    v2 = build_vector(-1, -1, 1);
    cube->tris[4] = build_triangle(v1, v2, v3);

    v1 = build_vector(-1, -1, 1);
    v3 = build_vector(-1, -1, -1);
    v2 = build_vector(-1, 1, -1);
    cube->tris[5] = build_triangle(v1, v2, v3);

    v1 = build_vector(-1, 1, -1);
    v3 = build_vector(-1, -1, -1);
    v2 = build_vector(1, 1, -1);
    cube->tris[6] = build_triangle(v1, v2, v3);

    v1 = build_vector(1, 1, -1);
    v3 = build_vector(-1, -1, -1);
    v2 = build_vector(1, -1, -1);
    cube->tris[7] = build_triangle(v1, v2, v3);

    v1 = build_vector(-1, 1, 1);
    v2 = build_vector(1, 1, 1);
    v3 = build_vector(-1, 1, -1);
    cube->tris[8] = build_triangle(v1, v2, v3);

    v1 = build_vector(1, 1, 1);
    v2 = build_vector(-1, 1, -1);
    v3 = build_vector(1, 1, -1);
    cube->tris[9] = build_triangle(v1, v2, v3);

    v1 = build_vector(-1, -1, 1);
    v2 = build_vector(1, -1, 1);
    v3 = build_vector(-1, -1, -1);
    cube->tris[10] = build_triangle(v1, v2, v3);

    v1 = build_vector(1, -1, 1);
    v2 = build_vector(1, -1, -1);
    v3 = build_vector(-1, -1, -1);
    cube->tris[11] = build_triangle(v1, v2, v3);

    return cube;
}

Model* build_model(Triangle* tris, int num_tris, Transform trans){
    Model* ret = (Model*) malloc(sizeof(Model));

    ret->tris= tris;
    ret->num_tris = num_tris;
    ret->transform = trans;
    return ret;
}

void render_model(Model* model, SH1106_t* dev){
    for(int i = 0; i < model->num_tris; i++){
        Triangle tri = transform_triangle(model->tris[i], model->transform);
        draw_triangle(dev, tri);
    }
}

void draw_triangle(SH1106_t* dev, Triangle tri){
    Vector v1 = project_vector(tri.v1);
    Vector v2 = project_vector(tri.v2);
    Vector v3 = project_vector(tri.v3);

    sh1106_draw_line(dev, v1.x, v1.y, v2.x, v2.y, true);
    sh1106_draw_line(dev, v2.x, v2.y, v3.x, v3.y, true);
    sh1106_draw_line(dev, v3.x, v3.y, v1.x, v1.y, true);
}

Vector project_vector(Vector p){
    double vport_x = (p.x * MAGNIFY) / p.z;
    double vport_y = (p.y * MAGNIFY) / p.z;

    int screen_x = round(vport_x + (SH1106_WIDTH / 2));
    int screen_y = round(vport_y + (SH1106_HEIGHT / 2));

    Vector ret = {
        .x = screen_x,
        .y = screen_y,
        .z = 1
    };
    return ret;
}

Vector vector_add(Vector v1, Vector v2){
    Vector ret = {
        .x = v1.x + v2.x,
        .y = v1.y + v2.y,
        .z = v1.z + v2.z
    };
    return ret;
}

Vector vector_scale(Vector v1, int scalar){
    Vector ret = {
        .x = v1.x * scalar,
        .y = v1.y * scalar,
        .z = v1.z * scalar
    };
    return ret;
}

Vector transform_vector(Vector v, Transform transform){
    Vector ret = v;
    double prev_x, prev_y;

    //scaling
    ret = vector_scale(ret, transform.scale);

    //x axis rotation
    prev_y = ret.y;
    ret.y = (ret.y * cos(transform.rot_x)) - (ret.z * sin(transform.rot_x));
    ret.z = (prev_y * sin(transform.rot_x)) + (ret.z * cos(transform.rot_x));

    //y axis rotation
    prev_x = ret.x;
    ret.x = (ret.x * cos(transform.rot_y)) + (ret.z * sin(transform.rot_y));
    ret.z = (ret.z * cos(transform.rot_y)) - (prev_x * sin(transform.rot_y));

    //z axis rotation
    prev_x = ret.x;
    ret.x = (ret.x * cos(transform.rot_z)) - (ret.y * sin(transform.rot_z));
    ret.y = (prev_x * sin(transform.rot_z)) + (ret.y * cos(transform.rot_z));

    //translation
    ret = vector_add(ret, transform.translation);


    return ret;
}

Vector build_vector(double x, double y, double z){
    Vector ret;
    ret.x = x;
    ret.y = y;
    ret.z = z;
    return ret; 
}

Triangle build_triangle(Vector v1, Vector v2, Vector v3){
    Triangle ret;
    ret.v1 = v1;
    ret.v2 = v2;
    ret.v3 = v3;
    return ret;
}

Triangle transform_triangle(Triangle tri, Transform transform){
    tri.v1 = transform_vector(tri.v1, transform);
    tri.v2 = transform_vector(tri.v2, transform);
    tri.v3 = transform_vector(tri.v3, transform);
    return tri;
}

esp_err_t i2c_master_init(){
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_SDA_PIN_NUM;
    conf.scl_io_num = I2C_SCL_PIN_NUM;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_CLK_SPEED;
    conf.clk_flags = 0;
    i2c_param_config(I2C_NUM_0, &conf);
    esp_err_t err;
    err = i2c_driver_install(I2C_NUM_0, conf.mode, 0, 0, 0);
    if(err != ESP_OK){
        ESP_LOGE("i2c_master_init", "Failed to configure i2c");
        return err;
    }
    return 0; 
}
