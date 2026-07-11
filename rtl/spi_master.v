// SPI master mode 0, octet par octet, diviseur programmable
// sclk = clk / (2*(div+1))   ex: div=62 -> 396.8 kHz, div=3 -> 6.25 MHz
module spi_master(
    input  wire clk,
    input  wire resetn,
    output reg  sclk,
    output wire mosi,
    input  wire miso,
    input  wire [7:0] div,
    input  wire [7:0] tx,
    input  wire start,
    output reg  [7:0] rx,
    output wire busy
);
    reg [7:0] shift;
    reg [4:0] half;      // 16 demi-periodes + repos
    reg [7:0] presc;
    assign busy = (half != 0);
    assign mosi = shift[7];

    always @(posedge clk) begin
        if (!resetn) begin
            sclk <= 0; half <= 0; shift <= 8'hFF; rx <= 8'hFF;
        end else if (half == 0) begin
            sclk <= 0;
            if (start) begin
                shift <= tx; half <= 16; presc <= div;
            end
        end else if (presc == 0) begin
            presc <= div;
            if (!sclk) begin
                sclk <= 1;                       // front montant: echantillonne
                rx <= {rx[6:0], miso};
            end else begin
                sclk <= 0;                       // front descendant: decale
                shift <= {shift[6:0], 1'b1};
                half <= half - 2;
            end
        end else
            presc <= presc - 1;
    end
endmodule
