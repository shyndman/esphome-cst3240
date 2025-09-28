import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor

from .. import cst3240_ns
from ..touchscreen import CST3240ButtonListener, CST3240Touchscreen

CONF_CST3240_ID = "cst3240_id"

CST3240Button = cst3240_ns.class_(
    "CST3240Button",
    binary_sensor.BinarySensor,
    cg.Component,
    CST3240ButtonListener,
    cg.Parented.template(CST3240Touchscreen),
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(CST3240Button).extend(
    {
        cv.GenerateID(CONF_CST3240_ID): cv.use_id(CST3240Touchscreen),
    }
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await cg.register_parented(var, config[CONF_CST3240_ID])
