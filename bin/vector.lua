-- ----------------------------------------------------------------------------
--
-- Generic Runner Game  Copyright (C) 2017  Andrew Janke
--
-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- (at your option) any later version.
--
-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.
--
-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.
--
-- ----------------------------------------------------------------------------

local vector = {}

local vector_metatable = {
    __add = function(l, r)
        return vector.new(l.x + r.x, l.y + r.y)
    end,
    __sub = function(l, r)
        return vector.new(l.x - r.x, l.y - r.y)
    end,
    __unm = function(u) return vector.new(-u.x, -u.y) end,
    __eq = function(l, r) return l.x == r.x and l.y == r.y end,
    __tostring = function(u)
        return "{ x = "..u.x..", y = "..u.y.." }"
    end,
    __mul = function(l, r)
        if type(l) == 'number' then
            return vector.new(l*r.x, l*r.y)
        elseif type(r) == 'number' then
            return vector.new(r*l.x, r*l.y)
        end
        error('Vectors may only be multiplied by scalars, for dot and '..
              '"cross" products, use a member method. If scalar '..
              'multiplication was intended, there was a type mismatch: '..
              'r: type == "'..type(r)..'" l: type "'..type(l)..'".'       )
    end,
    __div = function(l, r)
        if type(l) == 'number' then
            l, r = r, l
        end
        return mk_vector(l.x * (1.0 / r), l.y * (1.0 / r))
    end
}

vector.new = function(x_, y_)
    -- locals here AFAIK; are closed from public access
    -- do copy
    if x_ ~= nil and type(x_) == 'table' then
        if y_ == nil and x_.x ~= nil and x_.y ~= nil then
            local temp = x_
            x_ = temp.x
            y_ = temp.y
        end
    end
    -- default two param
    local self = {
        x = x_ or 0,
        y = y_ or 0
    }
    self.major = function()
        if math.abs(self.x) >= math.abs(self.y) then
            return vector.new(1, 0)
        else
            return vector.new(0, 1)
        end
    end
    self.magnitude = function()
        return math.sqrt(self.x*self.x + self.y*self.y)
    end
    self.major_with_magnitude = function()
        if math.abs(self.x) >= math.abs(self.y) then
            return vector.new(self.x, 0     )
        else
            return vector.new(0     , self.y)
        end
    end
    self.normal = function()
        local mag = self.magnitude()
        if mag == 0 then error('Cannot normalize the zero vector.') end
        return vector.new(self.x/mag, self.y/mag)
    end
    self.rounded = function()
        return vector.new(math.floor(self.x + 0.5), math.floor(self.y + 0.5))
    end
    self.rotate = function(angle)
        return vector.new(math.cos(angle)*self.x - math.sin(angle)*self.y ,
                          math.sin(angle)*self.x + math.cos(angle)*self.y )
    end
    self.rotate_randomly = function(range)
        local sign = 1.0
        if math.random(0, 1) == 1 then
            sign = -1.0
        end
        return self.rotate(v, (math.random(0, 1000)/1000.0)*range*sign)
    end
    self.dot = function(o) return self.x*o.x + self.y*o.y end
    self.project_unto = function(o)
        if o.magnitude() == 0 then
            error('Cannot project vector unto the zero vector.')
        end
        local scalar = self.dot(o) / (o.x*o.x + o.y*o.y)
        return scalar*o
    end
    self.cosine_similarity = function(o)
        return self.dot(o) / (o.magnitude()*self.magnitude())
    end
    self.angle_between = function(o)
        return math.acos(self.cosine_similarity(o))
    end
    -- gets the magnitude of the cross product, as if these vectors were in
    -- three space
    self.cross_magnitude = function(o) return self.x*o.y - self.y*o.x end
    setmetatable(self, vector_metatable)
    return self
end

-- vector has a set of utility functions
-- to keep this limited, a candidate utility function must:
-- - be strictly domain agnostic (specific to 2D geometory)
-- - useful in multiple points in the project's source code
--

vector.distance_to_line = function(a, b, v)
    -- distance from a point to a line
    -- derived from geometory
    -- more information available at:
    -- https://en.wikipedia.org/wiki/Distance_from_a_point_to_a_line
    local y_diff = b.y - a.y
    local x_diff = b.x - a.x
    if x_diff == 0 and y_diff == 0 then
        x_diff = a.x - v.x
        y_diff = a.y - v.y
        return math.sqrt(y_diff*y_diff + x_diff*x_diff)
    end
    return math.abs ( y_diff*v.x - x_diff*v.y + b.x*a.y - b.y*a.x ) /
           math.sqrt(        y_diff*y_diff + x_diff*x_diff        )
end

vector.is_vector = function(o)
    if type(o) ~= 'table' then return false end
    return getmetatable(o) == vector_metatable
end

return vector
