#ifndef BREEP_NETWORK_OBJECT_BUILDER_HPP
#define BREEP_NETWORK_OBJECT_BUILDER_HPP

///////////////////////////////////////////////////////////////////////////////////////////////////
//                                                                                               //
// Copyright 2017 Lucas Lazare.                                                                  //
// This file is part of Breep project which is released under the                                //
// European Union Public License v1.1. If a copy of the EUPL was                                 //
// not distributed with this software, you can obtain one at :                                   //
// https://joinup.ec.europa.eu/community/eupl/og_page/european-union-public-licence-eupl-v11     //
//                                                                                               //
///////////////////////////////////////////////////////////////////////////////////////////////////


/**
 * @file basic_network.hpp
 * @author Lucas Lazare
 * @since 0.1.0
 */

#include <mutex>
#include <functional>
#include <unordered_map>
#include <boost/archive/binary_iarchive.hpp>

#include "breep/util/type_traits.hpp"
#include "breep/util/logger.hpp"
#include "breep/network/typedefs.hpp"
#include "breep/network/basic_netdata_wrapper.hpp"

namespace breep {
	template <typename>
	class basic_network;
}

namespace breep { namespace detail {

	template <typename io_manager, typename T>
	class object_builder {
	public:

		using network = basic_network<io_manager>;
		using peer = typename network::peer;

		template <typename>
		using data_received_listener = std::function<void(basic_netdata_wrapper<io_manager, T>& data)>;

		object_builder()
				: m_listeners{}
				, m_to_add{}
			    , m_to_remove{}
				, m_mutex{}
		{}

		object_builder(const object_builder<T,network>&) = delete;
		object_builder(object_builder<T,network>&& other)
				: m_listeners(std::move(other.m_listeners))
				, m_mutex{}
		{}

		object_builder& operator=(const object_builder<T,network>&) = delete;

		bool build_and_call(network& lnetwork, const typename network::peer& received_from, boost::archive::binary_iarchive& data, bool is_private) {
			if (is_private) {
				breep::logger<object_builder<io_manager, T>>.info("Received " + type_traits<T>::universal_name());
			} else {
				breep::logger<object_builder<io_manager, T>>.info("Received private " + type_traits<T>::universal_name() + " from " + received_from.id_as_string());
			}

			m_mutex.lock();
			for (auto& pair : m_to_add) {
				breep::logger<object_builder<io_manager,T>>.trace("Effectively adding listener (id: " + std::to_string(pair.first) + ")");
				m_listeners.emplace(std::move(pair));
			}
			for (auto& id : m_to_remove) {
				breep::logger<object_builder<io_manager,T>>.trace("Effectively removing listener (id: " + std::to_string(id) + ")");
				m_listeners.erase(id);
			}
			m_mutex.unlock();

			if (m_listeners.empty()) {
				breep::logger<object_builder<io_manager,T>>.debug("No listener for received " + type_traits<T>::universal_name());
				return false;
			} else {
				breep::logger<object_builder<io_manager,T>>.debug("Bulding object of type " + type_traits<T>::universal_name());
				T object;
				data >> object;
				basic_netdata_wrapper<io_manager, T> wrapper(lnetwork, received_from, object, is_private);
				for (auto& listeners_pair : m_listeners) {
					breep::logger<object_builder<io_manager,T>>.trace("Calling listener with id " + std::to_string(listeners_pair.first));
					wrapper.listener_id = listeners_pair.first;
					listeners_pair.second(wrapper);
				}
				return true;
			}
		}

		type_listener_id add_listener(listener_id id, data_received_listener<T> l) {
			std::lock_guard<std::mutex> lock_guard(m_mutex);
			breep::logger<object_builder<io_manager,T>>.debug
					("Adding listener for type " + type_traits<T>::universal_name () + ". (id: " + std::to_string(id) + ")");
			m_to_add.push_back(std::pair<listener_id, data_received_listener<T>>(id, l));
			return type_listener_id(id, type_traits<T>::hash_code());
		}

		bool remove_listener(listener_id id) {
			if (m_listeners.count(id)) {
				std::lock_guard<std::mutex> lock_guard(m_mutex);
				if (std::find_if(m_to_remove.cbegin(), m_to_remove.cend(), [id](auto l_id) -> bool { return l_id == id; }) == m_to_remove.cend()) {
					breep::logger<object_builder<io_manager,T>>.debug
							("Removing listener for type " + type_traits<T>::universal_name () + ". (id: " + std::to_string(id) + ")");
					m_to_remove.push_back(id);
					return true;
				}
			} else {
				std::lock_guard<std::mutex> lock_guard(m_mutex);
				auto it = std::find_if(m_to_add.begin(), m_to_add.end(), [id](auto l_it) -> bool { return l_it.first == id; });
				if (it != m_to_add.cend()) {
					breep::logger<object_builder<io_manager,T>>.debug
							("Removing listener for type " + type_traits<T>::universal_name () + ". (id: " + std::to_string(id) + ")");
					*it = m_to_add.back();
					m_to_add.pop_back();
					return true;
				}
			}
			breep::logger<object_builder<io_manager,T>>.warning
					("Listener with id " + std::to_string(id) + " not found when trying to remove from listeners of type " + type_traits<T>::universal_name());
			return false;
		}

		void set_log_level(log_level ll) {
			breep::logger<object_builder<io_manager, T>>.level(ll);
		}

		void clear_any() {
			std::lock_guard<std::mutex> lock_guard(m_mutex);
			breep::logger<object_builder<io_manager, T>>.debug("Cleaning listeners list for type " + type_traits<T>::universal_name());
			m_listeners.clear();
			m_to_add.clear();
			m_to_remove.clear();
		}


	private:
		std::unordered_map<listener_id, data_received_listener<T>> m_listeners;
		std::vector<std::pair<listener_id, data_received_listener<T>>> m_to_add;
		std::vector<listener_id> m_to_remove;
		std::mutex m_mutex;
	};
}}
BREEP_DECLARE_TEMPLATE(breep::detail::object_builder)

#endif //BREEP_NETWORK_OBJECT_BUILDER_HPP
