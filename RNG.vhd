library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity RNG is
  port(
    clk : in std_logic;
    a   : in std_logic_vector(15 downto 0);  -- enable
    b   : in std_logic_vector(15 downto 0);  -- reset
    p   : out std_logic_vector(31 downto 0)
  );
end RNG;

architecture run_RNG of RNG is
signal Q: std_logic_vector(7 downto 0) := "00000001";
signal r: std_logic_vector(23 downto 0) := (others => '0');
--signal tmp: std_logic;

begin
process (clk)
variable tmp: STD_LOGIC := '0';
begin
if rising_edge(clk) then
    if (b > 0) then
        Q <= "00000001";
    elsif (a > 0) then
        tmp := Q(4) XOR Q(3) XOR Q(2) XOR Q(0);
        Q <= tmp & Q(7 downto 1);
    end if;
end if;
--    if a > 0 then
--    if rising_edge(clk) then
--    Q <= Q(2 downto 0) & tmp;
--    --Q <= Q + 1;
--    end if;
--    end if;
  end process;
p <= r & Q;
end run_RNG;
