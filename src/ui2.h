#include "util/types.h"

component {model : type} : type = (
    params : model
    update : ui_context -> model -> model
    draw : model -> back_context -> back_context
)

