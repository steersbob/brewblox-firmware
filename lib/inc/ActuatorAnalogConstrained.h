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
#include <algorithm>
#include <memory>
#include <vector>

namespace AAConstraints {
using value_t = ActuatorAnalog::value_t;

class Base {
public:
    // constraints are movable but not copyable
    Base() = default;
    Base(const Base&) = delete;
    Base& operator=(const Base&) = delete;
    Base(Base&&) = default;
    Base& operator=(Base&&) = default;

    virtual ~Base() = default;

    virtual value_t constrain(const value_t& val) const = 0;

    virtual uint8_t id() const = 0;

    virtual uint8_t order() const = 0;
};

template <uint8_t ID>
class Minimum : public Base {
private:
    value_t m_min;

public:
    Minimum(const value_t& v)
        : m_min(v)
    {
    }

    virtual value_t constrain(const value_t& val) const override final
    {
        return std::max(val, m_min);
    }

    virtual uint8_t id() const override final
    {
        return ID;
    }

    value_t min() const
    {
        return m_min;
    }

    virtual uint8_t order() const override final
    {
        return 0;
    }
};

template <uint8_t ID>
class Maximum : public Base {
private:
    value_t m_max;

public:
    Maximum(const value_t& v)
        : m_max(v)
    {
    }

    virtual value_t constrain(const value_t& val) const override final
    {
        return std::min(val, m_max);
    }

    virtual uint8_t id() const override final
    {
        return ID;
    }

    value_t max() const
    {
        return m_max;
    }

    virtual uint8_t order() const override final
    {
        return 1;
    }
};
}

/*
 * An ActuatorAnalog has a range output
 */

class ActuatorAnalogConstrained : public ActuatorAnalog {
public:
    using Constraint = AAConstraints::Base;

private:
    std::vector<std::unique_ptr<Constraint>> constraints;
    ActuatorAnalog& actuator;
    uint8_t m_limiting = 0x00;
    value_t m_desiredSetting = 0;

public:
    ActuatorAnalogConstrained(ActuatorAnalog& act)
        : actuator(act){};

    ActuatorAnalogConstrained(const ActuatorAnalogConstrained&) = delete;
    ActuatorAnalogConstrained& operator=(const ActuatorAnalogConstrained&) = delete;
    ActuatorAnalogConstrained(ActuatorAnalogConstrained&&) = default;
    ActuatorAnalogConstrained& operator=(ActuatorAnalogConstrained&&) = default;

    virtual ~ActuatorAnalogConstrained() = default;

    void addConstraint(std::unique_ptr<Constraint>&& newConstraint)
    {
        if (constraints.size() < 8) {
            constraints.push_back(std::move(newConstraint));
        }

        std::sort(constraints.begin(), constraints.end(),
                  [](const std::unique_ptr<Constraint>& a, const std::unique_ptr<Constraint>& b) { return a->order() < b->order(); });
    }

    void removeAllConstraints()
    {
        constraints.clear();
    }

    value_t constrain(const value_t& val)
    {
        // keep track of which constraints limit the setting in a bitfield
        m_limiting = 0x00;
        uint8_t bit = 0x01;

        value_t result = val;

        for (auto& c : constraints) {
            auto constrained = c->constrain(result);
            if (constrained != result) {
                m_limiting = m_limiting | bit;
                result = constrained;
            }
            bit = bit << 1;
        }

        return result;
    }

    virtual void setting(const value_t& val) override final
    {
        // first set actuator to requested value to check whether it constrains the setting itself
        actuator.setting(val);
        m_desiredSetting = actuator.setting();

        // then set it to the constrained value
        if (actuator.settingValid()) {
            actuator.setting(constrain(m_desiredSetting));
        } else {
            constrain(0);
        }
    }

    void update()
    {
        if (actuator.settingValid()) {
            setting(m_desiredSetting); // re-apply constraints
        }
    }

    virtual value_t setting() const override final
    {
        return actuator.setting();
    }

    virtual value_t value() const override final
    {
        return actuator.value();
    }

    virtual bool valueValid() const override final
    {
        return actuator.valueValid();
    }

    virtual bool settingValid() const override final
    {
        return actuator.settingValid();
    }

    virtual void settingValid(bool v) override final
    {
        auto old = actuator.settingValid();
        actuator.settingValid(v);
        if (old != actuator.settingValid()) {
            // update constraints state in case setting valid has changed the limits inside the actuator itself
            constrain(actuator.setting());
        }
    }

    value_t desiredSetting() const
    {
        return m_desiredSetting;
    }

    uint8_t limiting() const
    {
        return m_limiting;
    }

    const std::vector<std::unique_ptr<Constraint>>& constraintsList() const
    {
        return constraints;
    };
};