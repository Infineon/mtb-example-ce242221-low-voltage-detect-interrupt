/*******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the Low Voltage Detect (LVD)
*              interrupt example application for ModusToolbox.
*
*              The application monitors the VDDD supply rail using the on-chip
*              LVD block. When VDDD drops below the 2.8 V threshold a falling-
*              edge interrupt fires, sets a flag, and the main loop responds by
*              blinking LED2 (D8) five full cycles (10 toggles × 500 ms each).
*              LED1 (D11) blinks at 1 Hz continuously to show normal operation.
*
* Hardware setup:
*              - PSOC™ Control C3M/P8 KIT_PSC3M8_EVK
*              - An external power supply on the VDDD rail is required to
*                demonstrate the under-voltage event (drop from 3.3 V to < 2.8 V).
*
* Related Document: See README.md
*
********************************************************************************
* (c) 2026, Infineon Technologies AG, or an affiliate of Infineon
* Technologies AG. All rights reserved.
* This software, associated documentation and materials ("Software") is
* owned by Infineon Technologies AG or one of its affiliates ("Infineon")
* and is protected by and subject to worldwide patent protection, worldwide
* copyright laws, and international treaty provisions. Therefore, you may use
* this Software only as provided in the license agreement accompanying the
* software package from which you obtained this Software. If no license
* agreement applies, then any use, reproduction, modification, translation, or
* compilation of this Software is prohibited without the express written
* permission of Infineon.
*
* Disclaimer: UNLESS OTHERWISE EXPRESSLY AGREED WITH INFINEON, THIS SOFTWARE
* IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING, BUT NOT LIMITED TO, ALL WARRANTIES OF NON-INFRINGEMENT OF
* THIRD-PARTY RIGHTS AND IMPLIED WARRANTIES SUCH AS WARRANTIES OF FITNESS FOR A
* SPECIFIC USE/PURPOSE OR MERCHANTABILITY.
* Infineon reserves the right to make changes to the Software without notice.
* You are responsible for properly designing, programming, and testing the
* functionality and safety of your intended application of the Software, as
* well as complying with any legal requirements related to its use. Infineon
* does not guarantee that the Software will be free from intrusion, data theft
* or loss, or other breaches ("Security Breaches"), and Infineon shall have
* no liability arising out of any Security Breaches. Unless otherwise
* explicitly approved by Infineon, the Software may not be used in any
* application where a failure of the Product or any consequences of the use
* thereof can reasonably be expected to result in personal injury.
*******************************************************************************/


/*******************************************************************************
* Header Files
*******************************************************************************/

#include "cy_pdl.h"          /* PSoC PDL peripheral driver library             */
#include "cybsp.h"           /* Board support package – BSP pin/peripheral defs */
#include "cy_retarget_io.h"  /* Retarget printf/scanf to UART via HAL           */

/*******************************************************************************
* Macros
*******************************************************************************/

/* Number of full LED blink cycles (on + off = 1 cycle) performed by LED2
 * after an LVD interrupt is detected. Each half-cycle is 500 ms, so
 * LEDCYCLE = 5 produces 5 × 2 = 10 toggles → 5 complete blink cycles (5 s). */
#define LEDCYCLE          5

/*******************************************************************************
* Global Variables
*******************************************************************************/

/* Debug UART variables */
static cy_stc_scb_uart_context_t    DEBUG_UART_context;
static mtb_hal_uart_t               DEBUG_UART_hal_obj;

/* LVD interrupt configuration structure */
cy_stc_sysint_t LVD_IRQ_cfg  =
{
    .intrSrc      = srss_interrupt_IRQn,
    .intrPriority = 1,
};

/* Flag set by ISR_LVD() when an LVD falling-edge interrupt occurs.
 * The main loop polls this flag and clears it after handling the event. */
bool flagLVDIRQ = false;

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void ISR_LVD(void);   /* LVD interrupt service routine                    */

/*******************************************************************************
* Function Definitions
*******************************************************************************/

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  Application entry point. Performs the following sequence:
*    1. Initialises the board support package (clocks, power, GPIOs).
*    2. Configures and enables the debug SCB UART and retarget-IO layer so
*       that printf() output appears on the terminal.
*    3. Configures LVD to monitor VDDD with a 2.8 V falling-edge threshold
*       and registers ISR_LVD() as the interrupt handler.
*    4. Enters an infinite loop that:
*         - Blinks LED1 (D11) at 1 Hz to indicate normal operation.
*         - Detects the flagLVDIRQ flag set by ISR_LVD() and blinks
*           LED2 (D8) five times to signal the under-voltage event.
*
* Parameters:
*  void
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;
    cy_en_scb_uart_status_t init_status;

    /* -------------------------------------------------------------------------
     * Board / BSP initialisation
     * Configures system clocks, power domains, and all BSP-managed peripherals
     * (GPIOs, default pin drive modes, etc.).
     * ----------------------------------------------------------------------- */
    result = cybsp_init();
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);   /* Board init failed – halt execution in debug builds  */
    }

    /* -------------------------------------------------------------------------
     * Debug UART (SCB) initialisation
     * Initialise the SCB block in UART mode using the configuration generated
     * by Device Configurator (DEBUG_UART_config).
     * ----------------------------------------------------------------------- */
    init_status = Cy_SCB_UART_Init(DEBUG_UART_HW, &DEBUG_UART_config, &DEBUG_UART_context);
    if (init_status != CY_SCB_UART_SUCCESS)
    {
        CY_ASSERT(0);   /* SCB UART init failed – halt execution               */
    }

    /* Enable the SCB UART hardware block to begin TX/RX operation             */
    Cy_SCB_UART_Enable(DEBUG_UART_HW);

    /* Setup the HAL UART */
    result = mtb_hal_uart_setup(&DEBUG_UART_hal_obj, &DEBUG_UART_hal_config,
                                &DEBUG_UART_context, NULL);

    /* HAL UART init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);   /* HAL UART setup failed – halt execution              */
    }

    /* Initialize redirecting of low level IO */
    result = cy_retarget_io_init(&DEBUG_UART_hal_obj);
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);   /* Retarget-IO init failed – halt execution            */
    }

    /* -------------------------------------------------------------------------
     * LVD configuration
     * The LVD block must be disabled and its interrupt mask cleared before
     * changing any configuration registers to avoid spurious triggers.
     * ----------------------------------------------------------------------- */

    /* Disable LVD hardware while reconfiguring                                */
    Cy_LVD_Disable();

    /* Mask (block) LVD interrupt at the SRSS level before setup             */
    Cy_LVD_ClearInterruptMask();

    /* Select the voltage rail to monitor: VDDD (main supply rail)             */
    Cy_LVD_SetSourceVoltage(CY_LVD_SOURCE_VDDD);

    /* Set the under-voltage threshold to 2.8 V.
     * An interrupt will be generated when VDDD falls below this level.        */
    Cy_LVD_SetThreshold(CY_LVD_THRESHOLD_2_8_V);

    /* Pre-arm the LVD interrupt output so it can be captured by the NVIC      */
    Cy_LVD_SetInterrupt();

    /* Configure LVD to trigger on the falling edge of the comparator output
     * (i.e. when VDDD crosses the threshold from above to below 2.8 V).      */
    Cy_LVD_SetInterruptConfig(CY_LVD_INTR_FALLING);

    /* Re-enable LVD hardware; comparator output will become valid after ~25 µs */
    Cy_LVD_Enable();

    /* Wait at least 25 µs for the LVD comparator to settle before clearing
     * the interrupt register, preventing a false trigger on power-up.         */
    Cy_SysLib_DelayUs(25U);

    /* Clear any interrupt status that may have been set during LVD start-up   */
    Cy_LVD_ClearInterrupt();

    /* Unmask (allow) LVD interrupt at the SRSS level                        */
    Cy_LVD_SetInterruptMask();

    /* -------------------------------------------------------------------------
     * NVIC interrupt configuration for LVD
     * ----------------------------------------------------------------------- */

    /* Register ISR_LVD as the handler for the SRSS interrupt line           */
    Cy_SysInt_Init(&LVD_IRQ_cfg, ISR_LVD);

    /* Clear any stale pending interrupt in NVIC before enabling               */
    NVIC_ClearPendingIRQ(LVD_IRQ_cfg.intrSrc);

    /* Enable the LVD interrupt in NVIC */
    NVIC_EnableIRQ(LVD_IRQ_cfg.intrSrc);

    /* Transmit header to the terminal */
    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("************************************************************\r\n");
    printf("PSOC Control C3M/P8: low-voltage detection interrupt\r\n");
    printf("************************************************************\r\n\n");
    printf("1) LED1(D11) is blinking at 1Hz \r\n");
    printf("2) Drop external VDDD from 3.3V to below 2.8V and you will see LED2(D8) blink five times.\r\n");
    
    /* Enable global interrupts */
    __enable_irq();

    for (;;)
    {
        /* --- LVD event handling -------------------------------------------
         * ISR_LVD() sets flagLVDIRQ when VDDD drops below 2.8 V.
         * Blink LED2 (D8) LEDCYCLE times (5 on/off cycles = 10 toggles)
         * at 500 ms per toggle (1 Hz blink rate) to signal the LVD event.
         * The flag is cleared before entering the blink loop so a second
         * LVD event is not lost while blinking.
         * ----------------------------------------------------------------- */
        if (flagLVDIRQ)
        {
            flagLVDIRQ = 0;   /* Acknowledge the LVD event                   */

            /* Toggle LED2 LEDCYCLE*2 times → LEDCYCLE complete blink cycles */
            for (int i = 0; i < LEDCYCLE * 2; i++)
            {
                Cy_SysLib_Delay(500);                               /* 500 ms half-period */
                Cy_GPIO_Inv(CYBSP_USER_LED_PORT, CYBSP_USER_LED_PIN); /* Toggle LED2 (D8)  */
            }
        }

        /* --- Normal operation heartbeat -----------------------------------
         * Toggle LED1 (D11) every 500 ms to produce a 1 Hz blink, showing
         * that the CPU is running and no LVD event is pending.
         * ----------------------------------------------------------------- */
        Cy_SysLib_Delay(500);
        Cy_GPIO_Inv(CYBSP_USER_LED4_PORT, CYBSP_USER_LED4_PIN);   /* Toggle LED1 (D11) */
    }
}

/*******************************************************************************
* Function Name: ISR_LVD
********************************************************************************
* Summary:
*  Interrupt Service Routine for the LVD (SRSS combined interrupt) event.
*  Called by the NVIC when VDDD falls below the configured 2.8 V threshold on
*  a falling edge of the LVD comparator output.
*
*  Actions taken:
*    1. Sets flagLVDIRQ so main() can respond to the under-voltage event.
*    2. Clears the LVD interrupt status register to de-assert the IRQ line and
*       allow subsequent threshold crossings to generate new interrupts.
*
* Parameters:
*  void
*
* Return:
*  void
*
*******************************************************************************/
void ISR_LVD(void)
{
    /* Notify the main loop that an LVD under-voltage event has occurred       */
    flagLVDIRQ = true;

    /* Clear the LVD interrupt status register so the IRQ line is de-asserted
     * and the NVIC can accept the next LVD interrupt.                         */
    Cy_LVD_ClearInterrupt();
}

/* [] END OF FILE */
