/*
 * Copyright 2018 BrewPi B.V.
 *
 * This file is part of the BrewBlox Control Library.
 *
 * BrewPi is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * BrewPi is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with BrewPi.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include "ActuatorAnalog.h"
#include <functional>
#include <vector>

class Balanced;

class Balancer {
private:
    using value_t = ActuatorAnalog::value_t;

    struct Request {
        const Balanced* requester;
        value_t requested;
        value_t granted;
    };

    value_t available = 100;

    std::vector<Request> requesters;

public:
    Balancer() = default;
    virtual ~Balancer() = default;

    void registerEntry(const Balanced* req)
    {
        requesters.push_back(Request{req, 0, 0});
    }

    void unregisterEntry(const Balanced* req)
    {
        requesters.erase(std::remove_if(requesters.begin(), requesters.end(), [&req](const Request& r) { return r.requester == req; }));
    }

    value_t constrain(const Balanced* req, const value_t& val)
    {
        auto match = find_if(requesters.begin(), requesters.end(), [&req](const Request& r) {
            return r.requester == req;
        });

        if (match != requesters.end()) {
            match->requested = val;
            return std::min(val, match->granted);
        };
        return val;
    }

    void update()
    {
        value_t requestedTotal = 0;
        for (const auto& a : requesters) {
            requestedTotal += a.requested;
        }
        auto scale = available / std::max(available, requestedTotal);

        for (auto& a : requesters) {
            a.granted = a.requested * scale;
        }
    }
};

class Balanced {
private:
    using value_t = ActuatorAnalog::value_t;
    const std::function<std::shared_ptr<Balancer>()> m_balancer;

public:
    explicit Balanced(
        std::function<std::shared_ptr<Balancer>()>&& balancer)
        : m_balancer(balancer)
    {
        if (auto balancerPtr = balancer()) {
            balancerPtr->registerEntry(this);
        }
    }

    Balanced(const Balanced&) = delete;
    Balanced& operator=(const Balanced&) = delete;
    Balanced(Balanced&&) = default;
    Balanced& operator=(const Balanced&&) = delete;

    ~Balanced()
    {
        if (auto balancerPtr = m_balancer()) {
            balancerPtr->unregisterEntry(this);
        }
    }

    value_t operator()(const value_t& val) const
    {
        if (auto balancerPtr = m_balancer()) {
            return balancerPtr->constrain(this, val);
        }
        return val;
    }
};
