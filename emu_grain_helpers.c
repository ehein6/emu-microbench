/*! \file emu_grain_helpers
 \date March 15, 2018
 \author Eric Hein 
 \brief Source file for Emu grain size helpers
 */
#include "emu_grain_helpers.h"
#include <stdlib.h>

static replicated long gcs_per_nodelet = 0;
long
get_gcs_per_nodelet()
{
    if (gcs_per_nodelet == 0) {
        const char* str = getenv("GCS_PER_NODELET");
        if (str){
            mw_replicated_init(&gcs_per_nodelet, atol(str));
        } else {
            mw_replicated_init(&gcs_per_nodelet, 4);
        }
    }
    return gcs_per_nodelet;
}
