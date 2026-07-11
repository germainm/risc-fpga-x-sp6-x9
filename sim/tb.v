`timescale 1ns/1ps
// Banc d'essai complet: SoC + modele SD SPI (image FAT16) + decodeur UART
module tb;
    reg clk = 0;
    always #10 clk = ~clk;              // 50 MHz

    reg n_reset = 0;
    initial #200 n_reset = 1;

    wire txd1, sd_cs_n, sd_mosi, sd_sclk, sd_led, led;
    tri1 i2c_scl, i2c_sda;   // pullups seuls, EEPROM absente: repli attendu
    tri1 i2c2_scl, i2c2_sda; // second bus, libre
    tri1 [7:0] xgpio;        // port generique: pullups seuls, comme une entree flottante
    reg  rxd1 = 1;
    wire sd_miso;

    top dut(
        .clk(clk), .n_reset(n_reset), .rxd1(rxd1), .txd1(txd1),
        .sd_cs_n(sd_cs_n), .sd_mosi(sd_mosi), .sd_sclk(sd_sclk),
        .sd_miso(sd_miso), .sd_cd_n(1'b0), .sd_led(sd_led), .led(led),
        .i2c_scl(i2c_scl), .i2c_sda(i2c_sda),
        .i2c2_scl(i2c2_scl), .i2c2_sda(i2c2_sda),
        .xgpio(xgpio)
    );

    // ------------- modele de carte SD (SDHC, SPI mode 0) -------------
    integer fd;
    initial begin
        fd = $fopen("sd.img", "rb");
        if (fd == 0) begin $display("ERREUR: sd.img introuvable"); $finish; end
    end

    reg [7:0]  in_shift = 0;
    integer    bitcnt = 0;
    reg [7:0]  out_shift = 8'hFF;
    reg [47:0] cmdbuf;
    integer    cidx = 0;         // octets de commande restants
    reg [7:0]  q [0:7];          // file de reponse courte
    integer    qn = 0, qi = 0;
    integer    dcnt = 0;         // phase donnees (token+512+2crc)
    integer    dtot = 0;
    reg        acmd = 0;
    reg [31:0] arg;
    integer    tmp;

    assign sd_miso = sd_cs_n ? 1'b1 : out_shift[7];

    task automatic handle_cmd;
        reg [5:0] c;
        begin
            c   = cmdbuf[45:40];
            arg = cmdbuf[39:8];
            qn = 0; qi = 0;
            q[qn] = 8'hFF; qn = qn + 1;             // Ncr
            case (c)
                6'd0:  begin q[qn] = 8'h01; qn = qn + 1; end
                6'd8:  begin q[qn]=8'h01; qn=qn+1; q[qn]=8'h00; qn=qn+1;
                             q[qn]=8'h00; qn=qn+1; q[qn]=8'h01; qn=qn+1;
                             q[qn]=8'hAA; qn=qn+1; end
                6'd55: begin q[qn] = 8'h01; qn = qn + 1; acmd = 1; end
                6'd41: begin q[qn] = acmd ? 8'h00 : 8'h05; qn = qn + 1; acmd = 0; end
                6'd58: begin q[qn]=8'h00; qn=qn+1; q[qn]=8'hC0; qn=qn+1;  // OCR: CCS=1
                             q[qn]=8'hFF; qn=qn+1; q[qn]=8'h80; qn=qn+1;
                             q[qn]=8'h00; qn=qn+1; end
                6'd17: begin q[qn]=8'h00; qn=qn+1;
                             tmp = $fseek(fd, arg * 512, 0);
                             dcnt = 515; dtot = 515; end          // token+512+2
                default: begin q[qn] = 8'h04; qn = qn + 1; end    // illegal
            endcase
        end
    endtask

    function [7:0] next_out;
        input dummy;
        begin
            if (qi < qn) begin next_out = q[qi]; qi = qi + 1; end
            else if (dcnt > 0) begin
                if (dcnt == dtot)      next_out = 8'hFE;          // jeton
                else if (dcnt <= 2)    next_out = 8'hAA;          // CRC bidon
                else                   next_out = $fgetc(fd);
                dcnt = dcnt - 1;
            end else next_out = 8'hFF;
        end
    endfunction

    reg [7:0] rxbyte;
    always @(posedge sd_sclk) if (!sd_cs_n) begin
        in_shift = {in_shift[6:0], sd_mosi};
        bitcnt = bitcnt + 1;
        if (bitcnt == 8) begin
            bitcnt = 0;
            rxbyte = in_shift;
            if (cidx > 0) begin
                cmdbuf = {cmdbuf[39:0], rxbyte};
                cidx = cidx - 1;
                if (cidx == 0) handle_cmd;
            end else if (rxbyte[7:6] == 2'b01 && qi >= qn && dcnt == 0) begin
                cmdbuf = {40'b0, rxbyte};
                cidx = 5;
            end
        end
    end
    always @(negedge sd_sclk) if (!sd_cs_n) begin
        if (bitcnt == 0) out_shift = next_out(0);
        else out_shift = {out_shift[6:0], 1'b1};
    end
    always @(posedge sd_cs_n) begin bitcnt = 0; out_shift = 8'hFF; end

    // ------------- decodeur UART TX (115200) -------------
    localparam BT = 8680;   // ns par bit
    reg [7:0] ch;
    reg [63:0] last8 = 0;
    integer k;
    initial forever begin
        @(negedge txd1);
        #(BT + BT/2);
        for (k = 0; k < 8; k = k + 1) begin
            ch[k] = txd1;
            #BT;
        end
        $write("%c", ch); $fflush;
        last8 = {last8[55:0], ch};
        if (last8[47:0] == "blink:") begin
            $display("\n=== SUCCES: payload BLINK.BIN demarre ===");
            #1000 $finish;
        end
    end

    // ------------- stimulus: envoie '1' periodiquement -------------
    task send_char(input [7:0] c);
        integer j;
        begin
            rxd1 = 0; #BT;
            for (j = 0; j < 8; j = j + 1) begin rxd1 = c[j]; #BT; end
            rxd1 = 1; #BT;
        end
    endtask
    initial begin
        #6000000;                       // 6 ms: laisser init+menu s'afficher
        forever begin send_char("1"); #3000000; end
    end

    initial begin
        #120000000;                     // 120 ms max
        $display("\n=== TIMEOUT SIMULATION ===");
        $finish;
    end
endmodule
