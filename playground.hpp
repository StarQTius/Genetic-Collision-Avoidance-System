#ifndef GATE_PLAYGROUND_HPP
  #define GATE_PLAYGROUND_HPP

  #include <chrono> //system_clock
  #include <functional> //ref, reference_wrapper
  #include <list>
  #include <random> //default_random_engine, uniform_real_distribution

  #include <cmath> //M_PI, cos, sin, atan2, signbit, abs, max
  #define _USE_MATH_DEFINES

  #include <SFML/Graphics.hpp>

  #include "entity.hpp"
  #include "neural_network.hpp"

  static std::default_random_engine rnd_engine;

  namespace gate{

    /*
      Définitions pratiques
    */

    scalar relu(scalar x) { return std::max( (scalar) 0, x ); }

    typedef float scale_m1;
    typedef float distance_m;
    typedef float duration_s;
    typedef float speed_m_s;
    typedef float angle_rad;
    typedef float ang_speed_rad_s;

    typedef sf::Vector2<distance_m>                            Vector2;
    typedef sf::Rect<distance_m>                               Rect;
    typedef NeuralNetwork< std::function< decltype(relu) > >   ReLUNN;
    typedef std::reference_wrapper<std::default_random_engine> RandomEngineRef;
    typedef std::reference_wrapper<const ReLUNN>               NeuralNetworkRef;

    enum class Outcome{
      none,
      candidate_collided,
      foe_collided,
      goal_reached
    };

    struct Hitbox{

      Hitbox(const Vector2& pos, distance_m radius):
        pos{pos},
        radius{radius}
      {}

      /**

      **/

      bool is_in(const Rect& space) const {

        return space.left + radius < pos.x && space.top + radius < pos.y
            && pos.x < space.left + space.width - radius
            && pos.y < space.top + space.height - radius;

      }

      /**

      **/

      bool contains(const Vector2& point) const {

        auto relative_vect = pos - point;
        auto norm_m = std::hypot(relative_vect.x, relative_vect.y);
        return norm_m < radius;

      }

      /**

      **/

      bool is_intersecting(const Hitbox& hitbox) const {

        auto relative_vect = pos - hitbox.pos;
        auto norm_m = std::hypot(relative_vect.x, relative_vect.y);
        return norm_m < radius + hitbox.radius;

      }

      sf::Vector2<distance_m> pos;
      distance_m              radius;

    };

    enum Kind { candidate, foe };

    typedef Entity<distance_m, Hitbox, Outcome, Kind> Robot;


    /*
      Constantes
    */

    const scale_m1 px_per_m                   = 100;
    const size_t foe_nb                       = 2;
    const size_t input_vect_sz                = 2 * foe_nb + 6;
    const size_t output_vect_sz               = 3;
    const scalar is_moving_node_act_threshold = 0.7;
    const duration_s dt_s                     = 1e-3;
    const distance_m space_width              = 3;
    const distance_m space_height             = 2;
    const distance_m robot_radius_m           = 0.3;
    const speed_m_s robot_spd_m_s             = 1;
    const ang_speed_rad_s robot_ang_spd_rad_s = 2 * M_PI;
    const duration_s time_limit_s             = 100;

    /*
      Candidate : robot dont on souhaite tester le comportement
    */

    class Candidate{

    public:

      /*
        Apparence
      */

      static sf::Color fill_color;
      static sf::Color outline_color;

      /**

      **/

      Candidate(
        Vector2              pos,
        angle_rad            dir_rad,
        distance_m           radius_m,
        Vector2              objective,
        const ReLUNN&        neural_network):
        neural_network_ref{neural_network},
        hitbox{pos, radius_m},
        dir_rad{dir_rad},
        is_moving{false},
        objective{objective}
      {}

      /**

      **/

      Outcome update(
        const Rect&             space,
        const std::list<Robot>& foe_l)
      {

        for(const Robot& foe : foe_l){

          if(hitbox.is_intersecting(foe.shape()))
            return is_moving ?
              Outcome::candidate_collided
            : Outcome::foe_collided;

        }

        Vector input_vect{input_vect_sz};

        input_vect[0] = hitbox.pos.x;
        input_vect[1] = hitbox.pos.y;
        input_vect[2] = objective.x;
        input_vect[3] = objective.y;
        input_vect[4] = cos(dir_rad);
        input_vect[5] = sin(dir_rad);

        auto it = foe_l.begin();
        for(auto i = 0; i < foe_nb; i++){

          input_vect[2 * i + 4] = it->shape().pos.x;
          input_vect[2 * i + 5] = it->shape().pos.y;
          it++;

        }

        update_position(space, neural_network_ref(input_vect));

        return hitbox.contains(objective) ?
          Outcome::goal_reached
        : Outcome::none;

      }

      /**

      **/

      const Hitbox& shape() const {

        return hitbox;

      }

      /**

      **/

      Kind tag() const {

        return Kind::candidate;

      }

      /**

      **/

      void draw(sf::RenderTarget& rd_target, sf::RenderStates rd_states) const {

        sf::CircleShape sprite{px_per_m * hitbox.radius};

        sprite.setPosition(px_per_m * hitbox.pos);
        sprite.setFillColor(fill_color);
        sprite.setOutlineColor(outline_color);

        rd_target.draw(sprite);

      }

      /**

      **/

      void update_position(
        const Rect&   space,
        const Vector& output_vect)
      {

        is_moving = output_vect[0] < is_moving_node_act_threshold;

        if(is_moving){

          hitbox.pos.x += robot_spd_m_s * dt_s * cos(dir_rad);
          hitbox.pos.y += robot_spd_m_s * dt_s * sin(dir_rad);

        }
        else{

          Vector2 curr_dest{output_vect[1], output_vect[2]};
          Vector2 dplc_vect{curr_dest - hitbox.pos};

          auto ang_setpt_rad = atan2(dplc_vect.x, dplc_vect.y);
          auto ang_delta_rad = dir_rad - ang_setpt_rad;

          if(std::abs(ang_delta_rad) > robot_ang_spd_rad_s * dt_s){

            dir_rad += (std::signbit(ang_delta_rad) ? -1 : 1)
                     * robot_ang_spd_rad_s * dt_s;

          }
          else{

            dir_rad = ang_setpt_rad;

          }

        }

      }

    private:

      /*
        Variables membres
      */

      NeuralNetworkRef neural_network_ref;
      Hitbox           hitbox;
      angle_rad        dir_rad;
      bool             is_moving;
      Vector2          objective;

    };

    /*
      Foe : robot que le candidat doit éviter
    */

    class Foe{

    public:

      /*
        Apparence
      */

      static sf::Color fill_color;
      static sf::Color outline_color;

      /**

      **/

      Foe(
        Vector2                     pos,
        distance_m                  radius,
        std::default_random_engine& rnd_engine):
        hitbox{pos, radius},
        rnd_engine_ref{rnd_engine}
      {}

      /**

      **/

      Outcome update(
        const Rect&             space,
        const std::list<Robot>& candidate_l)
      {

        if(update_position()){

          std::uniform_real_distribution<distance_m> x_distr{
            hitbox.radius,
            space.width - hitbox.radius
          };

          std::uniform_real_distribution<distance_m> y_distr{
            hitbox.radius,
            space.height - hitbox.radius
          };

          curr_dest.x = x_distr(rnd_engine_ref.get());
          curr_dest.y = y_distr(rnd_engine_ref.get());

          auto delta_vect = hitbox.pos - curr_dest;
          auto delta_vect_norm_m = std::hypot(delta_vect.x, delta_vect.y);

          dir_cos = delta_vect.x / delta_vect_norm_m;
          dir_sin = delta_vect.y / delta_vect_norm_m;

        }

        return Outcome::none;

      }

      /**

      **/

      const Hitbox& shape() const {

        return hitbox;

      }

      /**

      **/

      Kind tag() const {

        return Kind::foe;

      }

      /**

      **/

      void draw(sf::RenderTarget& rd_target, sf::RenderStates rd_states) const {

        sf::CircleShape sprite{px_per_m * hitbox.radius};

        sprite.setPosition(px_per_m * hitbox.pos);
        sprite.setFillColor(fill_color);
        sprite.setOutlineColor(outline_color);

        rd_target.draw(sprite);

      }

      /**

      **/

      bool update_position(){

        hitbox.pos.x += robot_spd_m_s * dt_s * dir_cos;
        hitbox.pos.y += robot_spd_m_s * dt_s * dir_sin;

        auto delta_vect = curr_dest - hitbox.pos;
        return std::hypot(delta_vect.x, delta_vect.y) < robot_spd_m_s * dt_s;

      }

    private:

      /*
        Variables membres
      */

      Hitbox          hitbox;
      Vector2         curr_dest;
      angle_rad       dir_cos;
      angle_rad       dir_sin;
      RandomEngineRef rnd_engine_ref;

    };

    /*
      Trial : permet de simuler une épreuve
    */

    class Trial{

    public:

      /**

      **/

      Trial(std::default_random_engine::result_type seed):
        seed{seed}
      {}

      /**
        TODO : factoriser ça ptn
      **/

      duration_s operator()(const ReLUNN& neural_network){

        std::default_random_engine rnd_engine{seed};
        std::uniform_real_distribution<distance_m> pos_x_distr{
          robot_radius_m,
          space_width - robot_radius_m
        };
        std::uniform_real_distribution<distance_m> pos_y_distr{
          robot_radius_m,
          space_height - robot_radius_m
        };
        std::uniform_real_distribution<angle_rad> dir_distr{0, 2 * M_PI};

        Rect space{0, 0, space_width, space_height};

        Candidate candidate{
          {pos_x_distr(rnd_engine), pos_y_distr(rnd_engine)},
          dir_distr(rnd_engine),
          robot_radius_m,
          {pos_x_distr(rnd_engine), pos_y_distr(rnd_engine)},
          neural_network
        };

        std::list<Robot> foe_l;
        for(auto i = 0; i < foe_nb; i++){

          foe_l.emplace_back(
            Foe{
              {pos_x_distr(rnd_engine), pos_y_distr(rnd_engine)},
              robot_radius_m,
              rnd_engine
            }
          );

        }

        duration_s time_s = 0;
        Outcome outcome;
        while(
          ( outcome = candidate.update(space, foe_l) ) == Outcome::none
          && time_s < time_limit_s)
        {

          for(auto& foe : foe_l)
            foe.update(space, std::list<Robot>{candidate});

          time_s += dt_s;

        }

        switch(outcome){

          case Outcome::none :
          case Outcome::goal_reached :
            return time_s;
          case Outcome::candidate_collided : //Villain NN ! Villain !
            return time_limit_s;
          case Outcome::foe_collided :
            return (*this)(neural_network);

        }

      }

    private:

      /*
        Variables membres
      */

      const std::default_random_engine::result_type seed;

    };

  }

#endif
