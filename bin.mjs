import {Zcl} from 'zigbee-herdsman';
import * as m from 'zigbee-herdsman-converters/lib/modernExtend';
import {presets as e, access as ea} from 'zigbee-herdsman-converters/lib/exposes';
import * as utils from 'zigbee-herdsman-converters/lib/utils';
import * as constants from 'zigbee-herdsman-converters/lib/constants';

function binTimes() {
    const exposes = [];
    exposes.push(
        e.composite("display_times", "display_times", ea.SET)
            .withDescription("Next bin collection times")
            .withFeature(e.numeric("black", ea.SET))
            .withFeature(e.numeric("green", ea.SET))
            .withFeature(e.numeric("brown", ea.SET))
    );

    const toZigbee = [
        {
            key: ["display_times"],
            convertSet: async (entity, key, value, meta) => {
                utils.assertEndpoint(entity);

                const OneJan2000Secs = constants.OneJanuary2000 / 1000;
                const data = {
                    black: value.black != null && value.black > OneJan2000Secs ? value.black - OneJan2000Secs : 0,
                    green: value.green != null && value.green > OneJan2000Secs ? value.green - OneJan2000Secs : 0,
                    brown: value.brown != null && value.brown > OneJan2000Secs ? value.brown - OneJan2000Secs : 0
                };
                await entity.command("tcSpecificBin", "setDisplayTimes", data);

                return {state: {display_times: value}};
            }
        }
    ];

    return {exposes, toZigbee, isModernExtend: true};
}

export default {
    zigbeeModel: ['BinStatus'],
    model: 'BinStatus',
    vendor: 'TC',
    description: 'Custom bin status display',
    extend: [
        m.deviceAddCustomCluster("tcSpecificBin", {
            manufacturerCode: 0x1234,
            ID: 0xFC12,
            attributes: {},
            commands: {
                setDisplayTimes: {
                    ID: 0x01,
                    parameters: [
                        { name: "black", type: Zcl.DataType.UINT32 },
                        { name: "green", type: Zcl.DataType.UINT32 },
                        { name: "brown", type: Zcl.DataType.UINT32 },
                    ],
                },
            },
            commandsResponse: {},
        }),
        binTimes()
    ],
    ota: true
};
