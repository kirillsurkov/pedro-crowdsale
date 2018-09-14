#define SYMBOL_EOS eosio::string_to_symbol(4, "EOS")
#define SYMBOL_TKN eosio::string_to_symbol(DECIMALS, STR(SYMBOL))
#define SYMBOL_ETH eosio::string_to_symbol(4, "ETH")
#define SYMBOL_USD eosio::string_to_symbol(4, "USD")

#define ASSET_EOS(amount) eosio::extended_asset(eosio::asset(amount, SYMBOL_EOS), eosio::string_to_name("eosio.token"))
#define ASSET_TKN(amount) eosio::extended_asset(eosio::asset(amount, SYMBOL_TKN), eosio::string_to_name(STR(CONTRACT)))
#define ASSET_ETH(amount) eosio::asset(eosio::asset(amount, SYMBOL_ETH))
#define ASSET_USD(amount) eosio::asset(eosio::asset(amount, SYMBOL_USD))
