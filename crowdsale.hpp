#pragma once

#include <eosiolib/eosio.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/asset.hpp>

#include "config.h"
#include "pow10.h"
#include "str_expand.h"

class crowdsale : public eosio::contract {
private:
	struct multiplier_t {
		uint32_t num;
		uint32_t denom;
	};

	struct state_t {
		eosio::extended_asset total_eos;
		eosio::extended_asset total_tkn;
		eosio::asset total_eth;
		eosio::asset ethusd;
		eosio::asset eosusd;
		int32_t last_daily;
		time_t start;
		time_t finish;
#ifdef DEBUG
		time_t time;
#endif
	};

	// @abi table deposit
	struct deposit_t {
		account_name account;
		int64_t eoses;
		int64_t tokens;
		uint64_t primary_key() const { return account; }
	};

	// @abi table whitelist
	struct userlist_t {
		account_name account;
		uint64_t primary_key() const { return account; }
	};

	eosio::singleton<N(state), state_t> state_singleton;
	eosio::multi_index<N(deposit), deposit_t> deposits;
	eosio::multi_index<N(greylist), userlist_t> greylist;
	eosio::multi_index<N(whitelist), userlist_t> whitelist;

	eosio::extended_asset asset_eos;
	eosio::extended_asset asset_tkn;
	eosio::asset asset_eth;
	eosio::asset asset_usd;

	account_name issuer;

	state_t state;

	void on_deposit(account_name investor, eosio::asset quantity);

	state_t default_parameters() const {
		return state_t{
			.total_eos = this->asset_eos,
			.total_tkn = this->asset_tkn,
			.total_eth = this->asset_eth,
			.ethusd = this->asset_usd,
			.eosusd = this->asset_usd,
			.last_daily = 0,
			.start = 0,
			.finish = 0,
#ifdef DEBUG
			.time = 0
#endif
		};
	}

	void inline_issue(account_name to, eosio::extended_asset quantity, std::string memo) const {
		struct issue {
			account_name to;
			eosio::asset quantity;
			std::string memo;
		};
		eosio::action(
			eosio::permission_level(this->_self, N(active)),
			quantity.contract,
			N(issue),
			issue{to, quantity, memo}
		).send();
	}

	void inline_transfer(account_name from, account_name to, eosio::extended_asset quantity, std::string memo) const {
		struct transfer {
			account_name from;
			account_name to;
			eosio::asset quantity;
			std::string memo;
		};
		eosio::action(
			eosio::permission_level(this->_self, N(active)),
			quantity.contract,
			N(transfer),
			transfer{from, to, quantity, memo}
		).send();
	}

	void setgrey(account_name account) {
		auto it = this->greylist.find(account);
		eosio_assert(it == this->greylist.end(), "Account already greylisted");
		this->greylist.emplace(this->_self, [account](auto& e) {
			e.account = account;
		});
	}

	void unsetgrey(account_name account) {
		auto it = this->greylist.find(account);
		eosio_assert(it != this->greylist.end(), "Account not greylisted");
		greylist.erase(it);
	}

	void setwhite(account_name account) {
		auto it = this->whitelist.find(account);
		eosio_assert(it == this->whitelist.end(), "Account already whitelisted");
		this->whitelist.emplace(this->_self, [account](auto& e) {
			e.account = account;
		});
	}

	void unsetwhite(account_name account) {
		auto it = this->whitelist.find(account);
		eosio_assert(it != this->whitelist.end(), "Account not whitelisted");
		whitelist.erase(it);
	}

public:
	crowdsale(account_name self);
	~crowdsale();
	void transfer(uint64_t sender, uint64_t receiver);
	void init(time_t start, time_t finish);
	void setstart(time_t start);
	void setfinish(time_t finish);
	void grey(account_name account);
	void ungrey(account_name account);
	void greymany(eosio::vector<account_name> accounts);
	void ungreymany(eosio::vector<account_name> accounts);
	void white(account_name account);
	void unwhite(account_name account);
	void whitemany(eosio::vector<account_name> accounts);
	void unwhitemany(eosio::vector<account_name> accounts);
	void finalize();
	void withdraw();
	void setdaily(eosio::asset eth, eosio::asset ethusd, eosio::asset eosusd);
#ifdef DEBUG
	void settime(time_t time);
#endif
};
