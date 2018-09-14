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
	deposits(this->_self, this->_self),
	whitelist(this->_self, this->_self),
	greylist(this->_self, this->_self),
	asset_eos(
		eosio::asset(0, eosio::string_to_symbol(4, "EOS")),
		eosio::string_to_name("eosio.token")
	),
	asset_tkn(
		eosio::asset(0, eosio::string_to_symbol(DECIMALS, STR(SYMBOL))),
		eosio::string_to_name(STR(CONTRACT))
	),
	asset_eth(
		eosio::asset(0, eosio::string_to_symbol(4, "ETH"))
	),
	asset_usd(
		eosio::asset(0, eosio::string_to_symbol(4, "USD"))
	),
	issuer(eosio::string_to_name(STR(ISSUER))),
	state(state_singleton.exists() ? state_singleton.get() : default_parameters())
{
}

crowdsale::~crowdsale() {
	this->state_singleton.set(this->state, this->_self);
}

void crowdsale::on_deposit(account_name investor, eosio::asset quantity) {
	eosio_assert(NOWDAY == this->state.last_daily, "Rates not updated yet");
	eosio_assert(NOW >= this->state.start, "Crowdsale hasn't started");
	eosio_assert(NOW <= this->state.finish, "Crowdsale finished");

	eosio_assert(quantity.amount >= MIN_CONTRIB, "Contribution too low");
	eosio_assert((quantity.amount <= MAX_CONTRIB) || !MAX_CONTRIB, "Contribution too high");

	auto whitelist_it = this->whitelist.find(investor);
	eosio_assert(whitelist_it != this->whitelist.end(), "Account not whitelisted");

	auto deposits_it = this->deposits.find(investor);

	this->asset_tkn.set_amount(EOS2TKN(quantity.amount));

	this->state.total_eos += quantity;
	this->state.total_tkn += this->asset_tkn;

	eosio_assert(this->state.total_eos.amount <= HARD_CAP_EOS, "Hard cap reached");

	int64_t entire_eoses = quantity.amount;
	int64_t entire_tokens = this->asset_tkn.amount;
	if (deposits_it != this->deposits.end()) {
		entire_eoses += deposits_it->eoses;
		entire_tokens += deposits_it->tokens;
	}

	if (deposits_it == this->deposits.end()) {
		this->deposits.emplace(this->_self, [investor, entire_eoses, entire_tokens](auto& deposit) {
			deposit.account = investor;
			deposit.eoses = entire_eoses;
			deposit.tokens = entire_tokens;
		});
	} else {
		this->deposits.modify(deposits_it, this->_self, [investor, entire_eoses, entire_tokens](auto& deposit) {
			deposit.account = investor;
			deposit.eoses = entire_eoses;
			deposit.tokens = entire_tokens;
		});
	}

	this->inline_issue(investor, this->asset_tkn, "Crowdsale");
}

void crowdsale::init(time_t start, time_t finish) {
	eosio_assert(!this->state_singleton.exists(), "Already initialized");
	eosio_assert(start < finish, "Start must be less than finish");
	require_auth(this->_self);

	this->state.start = start;
	this->state.finish = finish;

	struct dest {
		account_name to;
		int64_t amount;
	} dests[MINTCNT];
	#define FILLDESTS(z, i, data)\
		dests[i] = dest{\
			eosio::string_to_name(STR(MINTDEST ## i)),\
			MINTVAL ## i\
		};
	BOOST_PP_REPEAT(MINTCNT, FILLDESTS, );

	for (int i = 0; i < MINTCNT; i++) {
		this->asset_tkn.set_amount(dests[i].amount);
		this->inline_issue(dests[i].to, this->asset_tkn, "Initial token distribution");
	}
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

void crowdsale::grey(account_name account) {
	require_auth(this->issuer);
	this->setgrey(account);
}

void crowdsale::ungrey(account_name account) {
	require_auth(this->issuer);
	this->unsetgrey(account);
}

void crowdsale::greymany(eosio::vector<account_name> accounts) {
	require_auth(this->issuer);
	for (account_name account : accounts) {
		this->setgrey(account);
	}
}

void crowdsale::ungreymany(eosio::vector<account_name> accounts) {
	require_auth(this->issuer);
	for (account_name account : accounts) {
		this->unsetgrey(account);
	}
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

void crowdsale::finalize() {
	eosio_assert(NOW > this->state.finish || this->state.total_eos.amount + MIN_CONTRIB + !MIN_CONTRIB >= HARD_CAP_EOS, "Crowdsale hasn't finished");
	eosio_assert(!TRANSFERABLE, "There is no reason to call finalize");

	struct unlock {
		eosio::symbol_type symbol;
	};
	eosio::action(
		eosio::permission_level(this->_self, N(active)),
		this->asset_tkn.contract,
		N(unlock),
		unlock{this->asset_tkn.symbol}
	).send();
}

void crowdsale::withdraw() {
	require_auth(this->issuer);
	this->inline_transfer(this->_self, this->issuer, this->state.total_eos, "Withdraw");
	this->state.total_eos.set_amount(0);
}

void crowdsale::setdaily(eosio::asset eth, eosio::asset ethusd, eosio::asset eosusd) {
	require_auth(this->issuer);

	eosio_assert(eth.symbol == this->asset_eth.symbol, "invalid ETH symbol");
	eosio_assert(ethusd.symbol == this->asset_usd.symbol, "invalid USD symbol for ETHUSD");
	eosio_assert(eosusd.symbol == this->asset_usd.symbol, "invalid USD symbol for EOSUSD");

	this->state.total_eth = eth;
	this->state.ethusd = ethusd;
	this->state.eosusd = eosusd;
	this->state.last_daily = NOWDAY;
}

#ifdef DEBUG
void crowdsale::settime(time_t time) {
	this->state.time = time;
}
EOSIO_ABI(crowdsale, (init)(setstart)(setfinish)(grey)(ungrey)(greymany)(ungreymany)(white)(unwhite)(whitemany)(unwhitemany)(finalize)(withdraw)(transfer)(settime));
#else
EOSIO_ABI(crowdsale, (init)(setstart)(setfinish)(grey)(ungrey)(greymany)(ungreymany)(white)(unwhite)(whitemany)(unwhitemany)(finalize)(withdraw)(transfer));
#endif
