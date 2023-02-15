#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>
#include <gpio/gpio.h>
#include <stdbool.h>
#include <bsp/bsp.h>
#include <sys/cdefs.h>
#include <rtl/countof.h>
#include <rtl/compiler.h>


#define GPIO_PIN_NUM_IN2 12U
#define GPIO_PIN_NUM_IN1 13U
#define GPIO_PIN_NUM_ENA 6U
#define GPIO_PIN_NUM_IN4 20U
#define GPIO_PIN_NUM_IN3 21U
#define GPIO_PIN_NUM_ENB 26U

#define GPIO_PIN_NUM_INTER_L 7U
#define GPIO_PIN_NUM_INTER_R 8U

#define GPIO_EVENT_DELAY_M  1000

#define HIGH 1
#define LOW 0

static GpioHandle handle = NULL;

#define _SET_GPIO_MODE(pin, mode) { \
   GpioOut(handle, pin, mode); \
}


#define LEFT_SET(in1, in2, mode) { \
   _SET_GPIO_MODE(GPIO_PIN_NUM_IN1, in1) \
   _SET_GPIO_MODE(GPIO_PIN_NUM_IN2, in2) \
   _SET_GPIO_MODE(GPIO_PIN_NUM_ENA, mode) \
}

#define RIGHT_SET(in4, in3, mode) { \
   _SET_GPIO_MODE(GPIO_PIN_NUM_IN3, in3) \
   _SET_GPIO_MODE(GPIO_PIN_NUM_IN4, in4) \
   _SET_GPIO_MODE(GPIO_PIN_NUM_ENB, mode) \
}


#define FORWARD(motor) {\
   motor##_SET(HIGH, LOW, HIGH) \
}

#define STOP(motor) { \
   motor##_SET(LOW, LOW, LOW) \
}

#define BACKWARD(motor) {\
   motor##_SET(LOW, HIGH, HIGH) \
}

void forward(){
   FORWARD(LEFT);
   FORWARD(RIGHT);
}

void stop() {
   STOP(LEFT);
   STOP(RIGHT);
}

void backward() {
   BACKWARD(LEFT);
   BACKWARD(RIGHT);
}

void left() {
   STOP(LEFT);
   FORWARD(RIGHT);
}

void right() {
    FORWARD(LEFT);
    STOP(RIGHT);
}


struct eventCntr {
    unsigned int ct;
    int cs;    
};

struct calcPos {
    struct eventCntr L;
    struct eventCntr R;
};

static struct calcPos currPos = { 
    {0, -1}, {0, -1}
};

#define _INCOME(pos, st) { \
    if (currPos.pos.cs != -1) currPos.pos.ct++; \
    currPos.pos.cs = st; \
}    

#define INCOME(event, pin) { \
   if (GPIO_EVENT_TYPE_EDGE_RISE == event ) { \
    if (pin == GPIO_PIN_NUM_INTER_L) { \
        _INCOME(L, 1); \
    } \
    if (pin == GPIO_PIN_NUM_INTER_R) { \
        _INCOME(R, 1); \
    }} \
}

#define _GET_CM(pos) (double)(1.021 * currPos.pos.ct) 

#define GET_CM_R _GET_CM(R)

#define GET_CM_L _GET_CM(L)

static int init() {
    BspError rc = BspInit(NULL);
    if (rc != BSP_EOK)
    {
        fprintf(stderr, "Failed to initialize BSP\n");
        return EXIT_FAILURE;
    }

    rc = BspSetConfig("gpio0", "raspberry_pi4b.default");
    if (rc != BSP_EOK)
    {
        fprintf(stderr, "Failed to set mux configuration for gpio0 module\n");
        return EXIT_FAILURE;
    }
    if (GpioInit()){
        fprintf(stderr, "GpioInit failed\n");
        return EXIT_FAILURE;
    }

    if (GpioOpenPort("gpio0", &handle) || handle == GPIO_INVALID_HANDLE)
    {
        fprintf(stderr, "GpioOpenPort failed\n");
        return EXIT_FAILURE;
    }

    GpioSetMode(handle, GPIO_PIN_NUM_IN1, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_IN2, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_IN3, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_IN4, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_ENA, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_ENB, GPIO_DIR_OUT);
    GpioSetMode(handle, GPIO_PIN_NUM_INTER_L, GPIO_DIR_IN | GPIO_EVENT_RISE_EDGE);
    GpioSetMode(handle, GPIO_PIN_NUM_INTER_R, GPIO_DIR_IN | GPIO_EVENT_RISE_EDGE);

    return 0;
}


static int destroy() {
    if(GpioClosePort(handle))
    {
        fprintf(stderr, "GpioClosePort failed\n");
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}


static Retcode GpioEventHandler()
{
    char buf[sizeof(KdfEvent) + sizeof(GpioEvent)] = { 0 };
    KdfEvent *event = (KdfEvent *) buf;
    Retcode rc = rcInvalidArgument;

    if (NULL != handle)
    {
        rc = GpioGetEvent(handle, event, GPIO_EVENT_DELAY_M);
        if (rcOk == rc)
        {
            rtl_uint32_t pin = *((rtl_uint32_t *) event->payload);
            INCOME(event->type, pin);          
        }
        else if (rcTimeout == rc)
        {
            /* Event hasn't occurred, yet. */
            rc = rcOk;
        }
    }

    return rc;
}

int main(int argc, const char *argv[])
{
    fprintf(stderr, "Start GPIO_output test\n");

    if (init() != 0) {
       return EXIT_FAILURE;
    };
    
    fprintf(stderr, "Starting move\n");
    forward();
    while (GET_CM_L < 100.0) {
        GpioEventHandler();
    } 
    stop();

    return destroy();
}
