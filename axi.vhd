    component RNG
    port (
      clk: in std_logic;
      a: in std_logic_VECTOR(15 downto 0);
      b: in std_logic_VECTOR(15 downto 0);
      p: out std_logic_VECTOR(31 downto 0));
    end component;
