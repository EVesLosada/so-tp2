#include <stdint.h>
#include <lib.h>
#include <moduleLoader.h>
#include <naiveConsole.h>

static uint64_t loadModule(uint8_t **module,
                           void *targetModuleAddress);
static uint32_t readUint32(uint8_t **address);

uint64_t loadModules(void *payloadStart,
                     void **targetModuleAddress)
{
    uint8_t *currentModule = (uint8_t *)payloadStart;
    uint32_t moduleCount = readUint32(&currentModule);

    uint64_t reservedEnd = (uint64_t)currentModule;

    for (uint32_t i = 0; i < moduleCount; i++) {
        uint64_t moduleEnd =
            loadModule(&currentModule, targetModuleAddress[i]);

        if (moduleEnd > reservedEnd)
            reservedEnd = moduleEnd;
    }

    /*
     * Reservar tambien la region donde estaban
     * empaquetados originalmente los modulos.
     */
    if ((uint64_t)currentModule > reservedEnd)
        reservedEnd = (uint64_t)currentModule;

    return reservedEnd;
}

static uint64_t loadModule(uint8_t **module,
                           void *targetModuleAddress)
{
    uint32_t moduleSize = readUint32(module);

    ncPrint("  Will copy module at 0x");
    ncPrintHex((uint64_t)*module);
    ncPrint(" to 0x");
    ncPrintHex((uint64_t)targetModuleAddress);
    ncPrint(" (");
    ncPrintDec(moduleSize);
    ncPrint(" bytes)");

    memcpy(targetModuleAddress, *module, moduleSize);
    *module += moduleSize;

    ncPrint(" [Done]");
    ncNewline();

    return (uint64_t)targetModuleAddress + moduleSize;
}

static uint32_t readUint32(uint8_t **address)
{
    uint32_t result = *(uint32_t *)(*address);
    *address += sizeof(uint32_t);
    return result;
}
