    component RNG
    port (
      clk: in std_logic;
      a: in std_logic_VECTOR(15 downto 0);
      b: in std_logic_VECTOR(15 downto 0);
      p: out std_logic_VECTOR(31 downto 0));
    end component;

        
        
    RNG_0 : RNG
    port map (
      clk => S_AXI_ACLK,
      a => slv_reg0(31 downto 16),
      b => slv_reg0(15 downto 0),
      p => RNG_out);
