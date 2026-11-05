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
#include <memoryManager.h>
#include <processManager.h>
#include <semaphore.h>
#include <pipe.h>

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

		/*
		* Inicializar el administrador Free List utilizando
		* la region fisica segura seleccionada durante el arranque.
		*/
		initializeMemoryManager(
			availableMemoryRegion.start,
			availableMemoryRegion.size
		);

		/*
		* Inicializar la tabla y el administrador de procesos.
		* Todavia no se crean ni ejecutan procesos nuevos.
		*/
		initializeProcessManager();
		initializeSemaphoreTable();
		initializePipeTable();

        /*
         * La shell debe ser un proceso real para poder
         * crear hijos y esperarlos mediante sysWait.
         *
         * Crear el proceso antes de habilitar las IRQ
         * evita interrupciones durante su inicializacion.
         */
        uint64_t shellPid = createProcess(
            "shell",
            (void (*)(void))sampleCodeModuleAddress,
            1,
            0,
            1
        );

        if (shellPid == 0) {
            terminalPrint("Error: no se pudo crear la shell.\n");

            for (;;) {
                __asm__ volatile ("cli; hlt");
            }
        }

        initializeInterrupts();

        /*
         * El scheduler inicia la shell.
         * El contexto original queda como contexto idle.
         */
        __asm__ volatile ("int $0x81" ::: "memory");

        for (;;) {
            __asm__ volatile ("sti; hlt" ::: "memory");
        }

        return 0;
}
