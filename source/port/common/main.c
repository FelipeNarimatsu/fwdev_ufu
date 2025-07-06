#include "hal.h"
#include "app.h"

static char *app_name = 0;

char *main_app_name_get(void)
{
    return app_name;
}

int main(int argc, char *argv[])
{
    app_name = malloc(strlen(argv[0]) + 1);
    strcpy(app_name, argv[0]);

    hal_init();
    app_init();
    
    while(app_terminate_get() == false)
    {
        // protect against from any other running threads and 
        // simulates a better behavior of code running from main (non interrupt context)
        uint32_t state = hal_cpu_critical_section_enter(HAL_CPU_CS_USER_LEVEL);
        app_loop();
        hal_cpu_critical_section_leave(state);
    }

    app_deinit();
    hal_deinit();

    free(app_name);
    
    return 0;
}
