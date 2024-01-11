#include <string.h>
#include "common/common.h"
#include "main.h"

int __entry_menu(int argc, char **argv)
{
    //! *******************************************************************
    //! *                 Jump to our application                    *
    //! *******************************************************************
    return Menu_Main();
}
