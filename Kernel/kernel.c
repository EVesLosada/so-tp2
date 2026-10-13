#include <stdint.h>
#include <string.h>
#include <lib.h>
#include <moduleLoader.h>
#include <naiveConsole.h>
#include <videoDriver.h>
#include <terminal.h>
#include <interrupts.h>
#include <memoryLayout.h>
#include <memoryRegion.h>

extern uint8_t text;
extern uint8_t rodata;
extern uint8_t data;
extern uint8_t bss;
extern uint8_t endOfKernelBinary;
extern uint8_t endOfKernel;
/*
 * Region que podra utilizar el administrador de memoria.
 * Se calcula durante el arranque, luego de limpiar BSS.
 */
static MemoryRegion availableMemoryRegion;
static uint8_t memoryRegionReady;

static void * const sampleCodeModuleAddress = (void *)USERLAND_MODULE_ADDRESS;

static void * const sampleDataModuleAddress = (void *)DATA_MODULE_ADDRESS;

typedef int (*EntryPoint)();


void clearBSS(void * bssAddress, uint64_t bssSize)
{
	memset(bssAddress, 0, bssSize);
}

void * getStackBase()
{
	return (void*)(
		(uint64_t)&endOfKernel
		+ PAGE_SIZE * KERNEL_STACK_PAGES
		- sizeof(uint64_t)
	);
}

void * initializeKernelBinary()
{
	char buffer[10];

	ncPrint("[x64BareBones]");
	ncNewline();

	ncPrint("CPU Vendor:");
	ncPrint(cpuVendor(buffer));
	ncNewline();

	ncPrint("[Loading modules]");
	ncNewline();
	void * moduleAddresses[] = {
		sampleCodeModuleAddress,
		sampleDataModuleAddress
	};

	uint64_t reservedEnd =
	loadModules(&endOfKernelBinary, moduleAddresses);
	ncPrint("[Done]");
	ncNewline();
	ncNewline();

	ncPrint("[Initializing kernel's binary]");
	ncNewline();

	clearBSS(&bss, &endOfKernel - &bss);
	/*
	* Reservar tambien las paginas del stack inicial.
	* El limite superior es exclusivo.
	*/
	uint64_t kernelStackEnd =
		(uint64_t)&endOfKernel +
		PAGE_SIZE * KERNEL_STACK_PAGES;

	if (kernelStackEnd > reservedEnd) {
		reservedEnd = kernelStackEnd;
	}

	/*
	* Seleccionar memoria fisica utilizable por encima
	* de todas las regiones ocupadas por el sistema.
	*/
	memoryRegionReady =
		selectMemoryRegion(reservedEnd, &availableMemoryRegion);

	ncPrint("  text: 0x");
	ncPrintHex((uint64_t)&text);
	ncNewline();
	ncPrint("  rodata: 0x");
	ncPrintHex((uint64_t)&rodata);
	ncNewline();
	ncPrint("  data: 0x");
	ncPrintHex((uint64_t)&data);
	ncNewline();
	ncPrint("  bss: 0x");
	ncPrintHex((uint64_t)&bss);
	ncNewline();

	ncPrint("[Done]");
	ncNewline();
	ncNewline();
	return getStackBase();
}

int main()
{
        //La terminal pertenece al kernel porque escribe en video.
        terminalInit();

		if (!memoryRegionReady) {
			terminalPrint("Error: no se encontro memoria fisica segura.\n");

			while (1) {
				__asm__ volatile ("cli; hlt");
			}
		}

        //Registramos int 80h para que Userland pueda solicitar servicios.
        initializeInterrupts();

        /*El modulo cargado en 0x400000 pasa a ser el inicio
         * de nuestra shell de Userland.*/
        ((EntryPoint) sampleCodeModuleAddress)();

        /*La shell no deberia retornar.
         * Si retorna por algun error, mantenemos el kernel activo.*/
        while (1) {
        }

        return 0;
}
