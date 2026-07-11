// SoC PicoRV32 - Spartan-6 XC6SLX9 TQG144 @ 50 MHz
// Carte memoire: ROM boot 16K @ 0x00000000, RAM 40K @ 0x00010000
// MMIO @ 0x80000000: UART, SPI (carte SD), LED
module top(
    input  wire clk,        // P126, 50 MHz
    input  wire n_reset,    // P114, bouton actif bas
    input  wire rxd1,       // P1
    output wire txd1,       // P2
    output wire sd_cs_n,    // P115
    output wire sd_mosi,    // P116
    output wire sd_sclk,    // P117
    input  wire sd_miso,    // P118
    input  wire sd_cd_n,    // P119 (non utilise par ce module SD)
    output wire sd_led,     // P121, activite SD
    output wire led,        // P133, LED usager (MMIO 0x20)
    output wire [10:0] gpio,// LCD1602 8 bits: D0..D7,RS,RW,E
    output wire [7:0] seg,  // 7 segments A..G,DP (actif bas)
    output wire [7:0] dig,  // selection chiffre BIT0..BIT7 (actif bas)
    output wire bell,       // P94
    input  wire [7:0] dip,  // DIP switch (pullups, ON = 0)
    output wire [11:0] leds,// LEDs D3..D14 de la carte
    input  wire [2:0] btn,  // K1,K2,K4 (pullups, presse = 0)
    inout  wire i2c_scl,    // P101, 24C04 (collecteur ouvert)
    inout  wire i2c_sda,    // P102
    inout  wire i2c2_scl,   // P5  - bus I2C libre pour capteurs externes
    inout  wire i2c2_sda,   // P7
    inout  wire [7:0] xgpio // port GPIO generique pour capteurs (P9,P10,P124,P132,P134,P138,P140,P142)
);
    // ---------- reset: POR + bouton synchronise ----------
    reg [7:0] por = 8'h00;
    reg [1:0] rst_sync;
    wire resetn = por[7];
    always @(posedge clk) begin
        rst_sync <= {rst_sync[0], n_reset};
        if (!rst_sync[1]) por <= 0;
        else if (!por[7]) por <= por + 1;
    end

    // ---------- CPU ----------
    wire        mem_valid, mem_instr;
    reg         mem_ready;
    wire [31:0] mem_addr, mem_wdata;
    wire [3:0]  mem_wstrb;
    reg  [31:0] mem_rdata;

    picorv32 #(
        .PROGADDR_RESET(32'h0000_0000),
        .STACKADDR(32'h0001_7F00),
        .ENABLE_COUNTERS(1),
        .ENABLE_COUNTERS64(0),
        .BARREL_SHIFTER(0),
        .COMPRESSED_ISA(0),
        .ENABLE_MUL(0),
        .ENABLE_DIV(0),
        .ENABLE_IRQ(0),
        .CATCH_MISALIGN(1),
        .CATCH_ILLINSN(1)
    ) cpu (
        .clk(clk), .resetn(resetn),
        .mem_valid(mem_valid), .mem_instr(mem_instr), .mem_ready(mem_ready),
        .mem_addr(mem_addr), .mem_wdata(mem_wdata),
        .mem_wstrb(mem_wstrb), .mem_rdata(mem_rdata),
        .mem_la_read(), .mem_la_write(), .mem_la_addr(), .mem_la_wdata(), .mem_la_wstrb(),
        .pcpi_valid(), .pcpi_insn(), .pcpi_rs1(), .pcpi_rs2(),
        .pcpi_wr(1'b0), .pcpi_rd(32'b0), .pcpi_wait(1'b0), .pcpi_ready(1'b0),
        .irq(32'b0), .eoi(),
        .trace_valid(), .trace_data(),
        .trap()
    );

    // ---------- decodage ----------
    wire sel_rom  = mem_valid && (mem_addr[31:16] == 16'h0000) && (mem_addr[15:14] == 2'b00); // 0x0000-0x3FFF
    wire sel_ram  = mem_valid && (mem_addr[31:16] == 16'h0001) && (mem_addr[15:0] < 16'h8000);
    wire sel_mmio = mem_valid && mem_addr[31];

    always @(posedge clk) begin
        if (!resetn) mem_ready <= 0;
        else mem_ready <= mem_valid && !mem_ready;
    end

    // ---------- ROM 16K (init par boot.hex) ----------
    (* rom_style = "block" *) reg [31:0] rom [0:4095];
    initial $readmemh("boot.hex", rom);
    reg [31:0] rom_q;
    always @(posedge clk) rom_q <= rom[mem_addr[13:2]];

    // ---------- RAM 40K, 4 voies d'octets (template BRAM XST: 1 always/voie) ----------
    wire [12:0] ridx = mem_addr[14:2];
    wire ram_wr = sel_ram && !mem_ready;

    (* ram_style = "block" *) reg [7:0] ram0 [0:8191]; reg [7:0] ram_q0;
    always @(posedge clk) begin
        if (ram_wr && mem_wstrb[0]) ram0[ridx] <= mem_wdata[7:0];
        ram_q0 <= ram0[ridx];
    end
    (* ram_style = "block" *) reg [7:0] ram1 [0:8191]; reg [7:0] ram_q1;
    always @(posedge clk) begin
        if (ram_wr && mem_wstrb[1]) ram1[ridx] <= mem_wdata[15:8];
        ram_q1 <= ram1[ridx];
    end
    (* ram_style = "block" *) reg [7:0] ram2 [0:8191]; reg [7:0] ram_q2;
    always @(posedge clk) begin
        if (ram_wr && mem_wstrb[2]) ram2[ridx] <= mem_wdata[23:16];
        ram_q2 <= ram2[ridx];
    end
    (* ram_style = "block" *) reg [7:0] ram3 [0:8191]; reg [7:0] ram_q3;
    always @(posedge clk) begin
        if (ram_wr && mem_wstrb[3]) ram3[ridx] <= mem_wdata[31:24];
        ram_q3 <= ram3[ridx];
    end
    wire [31:0] ram_q = {ram_q3, ram_q2, ram_q1, ram_q0};

    // ---------- peripheriques ----------
    reg  [7:0] uart_tx_data; reg uart_tx_start;
    wire [7:0] uart_rx_data; wire uart_rx_avail; reg uart_rx_pop;
    wire uart_tx_busy;
    uart #(.DIV(434)) u_uart (
        .clk(clk), .resetn(resetn), .txd(txd1), .rxd(rxd1),
        .tx_data(uart_tx_data), .tx_start(uart_tx_start), .tx_busy(uart_tx_busy),
        .rx_data(uart_rx_data), .rx_avail(uart_rx_avail), .rx_pop(uart_rx_pop)
    );

    reg [7:0] spi_div = 8'd62; reg spi_cs = 1'b1;
    reg [7:0] spi_tx; reg spi_start;
    wire [7:0] spi_rx; wire spi_busy;
    spi_master u_spi (
        .clk(clk), .resetn(resetn), .sclk(sd_sclk), .mosi(sd_mosi), .miso(sd_miso),
        .div(spi_div), .tx(spi_tx), .start(spi_start), .rx(spi_rx), .busy(spi_busy)
    );
    assign sd_cs_n = spi_cs;
    assign sd_led  = ~spi_cs;

    reg led_r = 0;
    assign led = led_r;

    reg [10:0] gpio_r = 0;
    assign gpio = gpio_r;

    reg scl_oe = 0, sda_oe = 0;         // 1 = tire la ligne a 0
    assign i2c_scl = scl_oe ? 1'b0 : 1'bz;
    assign i2c_sda = sda_oe ? 1'b0 : 1'bz;
    wire scl_in = i2c_scl;
    wire sda_in = i2c_sda;

    reg scl2_oe = 0, sda2_oe = 0;
    assign i2c2_scl = scl2_oe ? 1'b0 : 1'bz;
    assign i2c2_sda = sda2_oe ? 1'b0 : 1'bz;
    wire scl2_in = i2c2_scl;
    wire sda2_in = i2c2_sda;

    // ---------- GPIO generique 8 bits (style DDR/PORT/PIN) ----------
    reg [7:0] gpx_dir = 0;    // 1 = sortie
    reg [7:0] gpx_out = 0;
    genvar gi;
    generate
        for (gi = 0; gi < 8; gi = gi + 1) begin : GPX
            assign xgpio[gi] = gpx_dir[gi] ? gpx_out[gi] : 1'bz;
        end
    endgenerate
    reg [7:0] gpx_s0, gpx_s1;
    always @(posedge clk) begin gpx_s0 <= xgpio; gpx_s1 <= gpx_s0; end

    localparam LEDS_ACTIVE_LOW = 1'b0;  // inverser si tout s'allume au boot
    reg [11:0] leds_r = 0;
    assign leds = LEDS_ACTIVE_LOW ? ~leds_r : leds_r;


    // ---------- afficheur 7 segments: scanner materiel ----------
    localparam SEG_ACTIVE_LOW = 1'b1;   // inverser si affichage negatif
    localparam DIG_ACTIVE_LOW = 1'b1;
    (* ram_style = "distributed" *) reg [7:0] disp [0:7];
    integer di;
    initial for (di = 0; di < 8; di = di + 1) disp[di] = 8'h00;
    reg [15:0] scan_div = 0;
    reg [2:0]  scan_idx = 0;
    always @(posedge clk) begin
        scan_div <= scan_div + 1;
        if (scan_div == 16'hFFFF) scan_idx <= scan_idx + 1;
    end
    wire [7:0] seg_raw = disp[scan_idx];
    wire [7:0] dig_raw = (scan_div < 16'd512) ? 8'h00 : (8'h01 << scan_idx); // blanking anti-fantome
    assign seg = SEG_ACTIVE_LOW ? ~seg_raw : seg_raw;
    assign dig = DIG_ACTIVE_LOW ? ~dig_raw : dig_raw;

    // ---------- cloche: generateur de tonalite ----------
    // ecrire (F_CPU/2)/freq dans [15:0]; 0 = silence; bit31 = niveau continu
    reg [31:0] bell_ctl = 0;
    reg [15:0] bell_cnt = 0;
    reg bell_r = 0;
    assign bell = bell_ctl[31] ? 1'b1 : (bell_ctl[15:0] != 0 ? bell_r : 1'b0);
    always @(posedge clk) begin
        if (bell_ctl[15:0] != 0) begin
            if (bell_cnt >= bell_ctl[15:0]) begin bell_cnt <= 0; bell_r <= ~bell_r; end
            else bell_cnt <= bell_cnt + 1;
        end
    end

    // ---------- DIP + boutons: synchronisation 2 etages ----------
    reg [7:0] dip_s0, dip_s1;
    reg [2:0] btn_s0, btn_s1;
    always @(posedge clk) begin
        dip_s0 <= dip; dip_s1 <= dip_s0;
        btn_s0 <= btn; btn_s1 <= btn_s0;
    end

    // ecriture MMIO
    always @(posedge clk) begin
        uart_tx_start <= 0; spi_start <= 0; uart_rx_pop <= 0;
        if (!resetn) begin
            spi_cs <= 1'b1; spi_div <= 8'd62; led_r <= 0; gpio_r <= 0; leds_r <= 0; scl_oe <= 0; sda_oe <= 0; scl2_oe <= 0; sda2_oe <= 0; gpx_dir <= 0; gpx_out <= 0;
        end else if (sel_mmio && !mem_ready) begin
            if (mem_wstrb != 0) begin
                casez (mem_addr[7:0])
                    8'h00: begin uart_tx_data <= mem_wdata[7:0]; uart_tx_start <= 1; end
                    8'h10: begin spi_tx <= mem_wdata[7:0]; spi_start <= 1; end
                    8'h18: begin spi_cs <= mem_wdata[0]; spi_div <= mem_wdata[15:8]; end
                    8'h20: led_r <= mem_wdata[0];
                    8'h24: leds_r <= mem_wdata[11:0];
                    8'h2C: begin scl_oe <= mem_wdata[0]; sda_oe <= mem_wdata[1]; end
                    8'h60: begin scl2_oe <= mem_wdata[0]; sda2_oe <= mem_wdata[1]; end
                    8'h64: gpx_dir <= mem_wdata[7:0];
                    8'h68: gpx_out <= mem_wdata[7:0];
                    8'h30: gpio_r <= mem_wdata[10:0];
                    8'h38: bell_ctl <= mem_wdata;
                    8'b010zzz00: disp[mem_addr[4:2]] <= mem_wdata[7:0];
                endcase
            end else if (mem_addr[7:0] == 8'h00) begin
                uart_rx_pop <= 1;   // lecture data = pop
            end
        end
    end

    // lecture MMIO (registree, alignee sur mem_ready)
    reg [31:0] mmio_q;
    always @(posedge clk) begin
        casez (mem_addr[7:0])
            8'h00: mmio_q <= {24'b0, uart_rx_data};
            8'h04: mmio_q <= {30'b0, uart_tx_busy, uart_rx_avail};
            8'h10: mmio_q <= {24'b0, spi_rx};
            8'h14: mmio_q <= {31'b0, spi_busy};
            8'h20: mmio_q <= {31'b0, led_r};
            8'h24: mmio_q <= {20'b0, leds_r};
            8'h28: mmio_q <= {29'b0, btn_s1};
            8'h2C: mmio_q <= {28'b0, scl_in, sda_in, sda_oe, scl_oe};
            8'h60: mmio_q <= {28'b0, scl2_in, sda2_in, sda2_oe, scl2_oe};
            8'h64: mmio_q <= {24'b0, gpx_dir};
            8'h68: mmio_q <= {24'b0, gpx_out};
            8'h6C: mmio_q <= {24'b0, gpx_s1};
            8'h30: mmio_q <= {21'b0, gpio_r};
            8'h38: mmio_q <= bell_ctl;
            8'h3C: mmio_q <= {24'b0, dip_s1};
            8'b010zzz00: mmio_q <= {24'b0, disp[mem_addr[4:2]]};
            default: mmio_q <= 32'h0;
        endcase
    end

    // mux de lecture (les *_q sont valides au cycle de mem_ready)
    reg sel_rom_q, sel_ram_q;
    always @(posedge clk) begin
        sel_rom_q <= sel_rom; sel_ram_q <= sel_ram;
    end
    always @(*) begin
        if (sel_rom_q)      mem_rdata = rom_q;
        else if (sel_ram_q) mem_rdata = ram_q;
        else                mem_rdata = mmio_q;
    end
endmodule
