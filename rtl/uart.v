// UART 115200 8N1 - TX + RX, divisor fixe
module uart #(
    parameter DIV = 434  // 50 MHz / 115200
)(
    input  wire clk,
    input  wire resetn,
    output reg  txd,
    input  wire rxd,
    input  wire [7:0] tx_data,
    input  wire tx_start,
    output wire tx_busy,
    output reg  [7:0] rx_data,
    output reg  rx_avail,
    input  wire rx_pop
);
    // ---------- TX ----------
    reg [9:0]  tx_shift;
    reg [3:0]  tx_cnt;      // bits restants
    reg [15:0] tx_div;
    assign tx_busy = (tx_cnt != 0);

    always @(posedge clk) begin
        if (!resetn) begin
            txd <= 1'b1; tx_cnt <= 0; tx_div <= 0; tx_shift <= 10'h3FF;
        end else if (tx_cnt == 0) begin
            txd <= 1'b1;
            if (tx_start) begin
                tx_shift <= {1'b1, tx_data, 1'b0}; // stop, data, start
                tx_cnt <= 10; tx_div <= DIV-1;
            end
        end else begin
            txd <= tx_shift[0];
            if (tx_div == 0) begin
                tx_shift <= {1'b1, tx_shift[9:1]};
                tx_cnt <= tx_cnt - 1; tx_div <= DIV-1;
            end else
                tx_div <= tx_div - 1;
        end
    end

    // ---------- RX ----------
    reg [1:0]  rx_sync;
    reg [3:0]  rx_cnt;
    reg [15:0] rx_div;
    reg [7:0]  rx_shift;
    wire rxd_s = rx_sync[1];

    always @(posedge clk) begin
        if (!resetn) begin
            rx_sync <= 2'b11; rx_cnt <= 0; rx_avail <= 0;
        end else begin
            rx_sync <= {rx_sync[0], rxd};
            if (rx_pop) rx_avail <= 0;
            if (rx_cnt == 0) begin
                if (!rxd_s) begin           // front de start
                    rx_cnt <= 10; rx_div <= DIV/2;  // echantillonner au milieu
                end
            end else if (rx_div == 0) begin
                rx_div <= DIV-1;
                if (rx_cnt == 10) begin
                    if (rxd_s) rx_cnt <= 0; // faux start
                    else rx_cnt <= rx_cnt - 1;
                end else if (rx_cnt == 1) begin
                    rx_cnt <= 0;
                    if (rxd_s) begin        // stop valide
                        rx_data <= rx_shift; rx_avail <= 1;
                    end
                end else begin
                    rx_shift <= {rxd_s, rx_shift[7:1]};
                    rx_cnt <= rx_cnt - 1;
                end
            end else
                rx_div <= rx_div - 1;
        end
    end
endmodule
