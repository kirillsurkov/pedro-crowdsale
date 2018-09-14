#include "crowdsale.hpp"
#include "override.hpp"

#define EOS2TKN(EOS) (int64_t)((EOS) * POW10(DECIMALS) * RATE / (1.0 * POW10(4) * RATE_DENOM))

#ifdef DEBUG
#define NOW this->state.time
#else
#define NOW now()
#endif

#define NOWDAY NOW / 86400

crowdsale::crowdsale(account_name self) :
	eosio::contract(self),
	state_singleton(this->_self, this->_self),
	whitelist(this->_self, this->_self),
	issuer(eosio::string_to_name(STR(ISSUER))),
	state(state_singleton.exists() ? state_singleton.get() : default_parameters())
{
}

crowdsale::~crowdsale() {
	this->state_singleton.set(this->state, this->_self);
}

void crowdsale::on_deposit(account_name investor, eosio::extended_asset quantity) {
	eosio_assert(NOWDAY == this->state.last_daily, "Rates not updated yet");
	eosio_assert(NOW >= this->state.start, "Crowdsale hasn't started");
	eosio_assert(NOW <= this->state.finish, "Crowdsale finished");

	eosio_assert(quantity.amount >= MIN_CONTRIB, "Contribution too low");
	eosio_assert((quantity.amount <= MAX_CONTRIB) || !MAX_CONTRIB, "Contribution too high");

	if (this->whitelist.find(investor) != this->whitelist.end()) {
		this->state.total_eos += quantity;
	}

	eosio_assert(this->state.total_eos.amount <= HARD_CAP_EOS, "Hard cap reached");

	deposits deposits_table(this->_self, investor);
	deposits_table.emplace(this->_self, [&](auto& deposit) {
		deposit.pk = deposits_table.available_primary_key();
		deposit.eos = quantity;
		deposit.eosusd = this->state.eosusd;
	});
}

void crowdsale::init(time_t start, time_t finish) {
	eosio_assert(!this->state_singleton.exists(), "Already initialized");
	eosio_assert(start < finish, "Start must be less than finish");

	require_auth(this->_self);

	this->state.start = start;
	this->state.finish = finish;

	#define PREMINT(z, i, data) \
		this->inline_issue(\
			eosio::string_to_name(STR(MINTDEST ## i)),\
			ASSET_TKN(MINTVAL ## i),\
			"Initial token distribution"\
		);
	BOOST_PP_REPEAT(MINTCNT, PREMINT, );
}

void crowdsale::setstart(time_t start) {
	eosio_assert(NOW <= this->state.start, "Crowdsale already started");
	require_auth(this->issuer);
	this->state.start = start;
}

void crowdsale::setfinish(time_t finish) {
	eosio_assert(NOW <= this->state.finish, "Crowdsale finished");
	require_auth(this->issuer);
	this->state.finish = finish;
}

void crowdsale::white(account_name account) {
	require_auth(this->issuer);
	this->setwhite(account);
}

void crowdsale::unwhite(account_name account) {
	require_auth(this->issuer);
	this->unsetwhite(account);
}

void crowdsale::whitemany(eosio::vector<account_name> accounts) {
	require_auth(this->issuer);
	for (account_name account : accounts) {
		this->setwhite(account);
	}
}

void crowdsale::unwhitemany(eosio::vector<account_name> accounts) {
	require_auth(this->issuer);
	for (account_name account : accounts) {
		this->unsetwhite(account);
	}
}

void crowdsale::withdraw() {
	require_auth(this->issuer);
	this->inline_transfer(this->_self, this->issuer, this->state.total_eos, "Withdraw");
	this->state.total_eos.set_amount(0);
}

void crowdsale::setdaily(eosio::asset eth, eosio::asset ethusd, eosio::asset eosusd) {
	require_auth(this->issuer);

	eosio_assert(eth.symbol == SYMBOL_ETH, "Invalid ETH symbol");
	eosio_assert(ethusd.symbol == SYMBOL_USD, "Invalid USD symbol for ETHUSD");
	eosio_assert(eosusd.symbol == SYMBOL_USD, "Invalid USD symbol for EOSUSD");

	this->state.total_eth = eth;
	this->state.ethusd = ethusd;
	this->state.eosusd = eosusd;
	this->state.last_daily = NOWDAY;
}

#ifdef DEBUG
void crowdsale::settime(time_t time) {
	this->state.time = time;
}
EOSIO_ABI(crowdsale, (init)(setstart)(setfinish)(white)(unwhite)(whitemany)(unwhitemany)(withdraw)(transfer)(settime));
#else
EOSIO_ABI(crowdsale, (init)(setstart)(setfinish)(white)(unwhite)(whitemany)(unwhitemany)(withdraw)(transfer));
#endif
