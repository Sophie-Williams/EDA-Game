#include "Player.hh"
#include <stack>

#define PLAYER_NAME Tixer

struct PLAYER_NAME : public Player {

  static Player* factory () {
    return new PLAYER_NAME;
  }

  /**
   * Types and attributes for your player can be defined here.
   */

  struct point {
    int x, y, xa, ya;

    point() {}
    point (int x, int y, int xa, int ya) {
      this-> x = x;
      this-> y = y;
      this-> xa = xa;
      this-> ya = ya;
    }

    friend bool operator< (const point& a, const point& b) {
      if (a.x != b.x) return a.x < b.x;
      return a.y < b.y;
    }
  };

  struct Comp {
      bool operator () (const pair<double, point> &a, const pair<double, point> &b) {
          if (a.first == b.first) return a.second < b.second;
          return a.first < b.first;
      }
  };
    
  struct dks_cons {
    bool any_city, a_path, mine;
    int id, attacker;

    dks_cons() {}
    dks_cons (bool any_city, bool a_path, bool mine, int id, int attacker) {
      this-> any_city = any_city;
      this-> a_path = a_path;
      this-> mine = mine;
      this-> id = id;
      this-> attacker = attacker;
    }
  };

  typedef vector<Pos> VP;
  typedef vector<int> VI;
  typedef vector<double> VD;
  typedef vector<VI>  VVI;
  typedef vector<VD>  VVD;
  
  vector< stack<point> > ork_paths;
  VP should_be;
  VI defensant;
  vector<pair<int, int>> orders; // order, id
  //  i  order              id
  //  -  -----              --
  //  0  go to any city     -1
  //  1  defend city        city_id
  //  2  attacking in city  ork_id
  //  3  recapturar path    city_id (la que estava)

  VVI ork_at;
  VP my_ork_at;

  VVI orks_in_city;
  VVI orks_in_paths;

  VI cities;
  VI paths;

  VI ids;

  bool is_my_ork(int id) {
    return unit(id).player == me();
  }

  bool s_cell_thread(int x, int y, int my_health) {
    Cell cel = cell(x, y);
    if (cel.unit_id != -1 and not is_my_ork(cel.unit_id) and unit(cel.unit_id).health > my_health) return true;
    else return false;
  }

  bool cell_thread(const Pos& p, int id) {
    bool thread = false;
    int my_health = unit(id).health;
    return s_cell_thread(p.i, p.j, my_health) or s_cell_thread(p.i-1, p.j, my_health) or s_cell_thread(p.i+1, p.j, my_health) or s_cell_thread(p.i, p.j-1, my_health) or s_cell_thread(p.i, p.j+1, my_health);
  }

  // Moves ork with identifier id.
  void move(int id, const point& P) {
    Unit u = unit(id);
    Pos pos = u.pos;
    Pos npos = pos;
    Dir dir = Dir(NONE);

    if (pos.i == P.x and pos.j == P.y-1) {
      dir = Dir(RIGHT);
      npos = pos + dir;
    } else if (pos.i == P.x and pos.j == P.y+1) {
      dir = Dir(LEFT);
      npos = pos + dir;
    } else if (pos.i-1 == P.x and pos.j == P.y) {
      dir = Dir(TOP);
      npos = pos + dir;
    } else if (pos.i+1 == P.x and pos.j == P.y) {
      dir = Dir(BOTTOM);
      npos = pos + dir;
    }

    int my_health = unit(id).health;
    if (cell_thread(npos, id)) {
      dir = Dir((dir+1)%4);
      npos = pos + dir;
      if (cell_thread(npos, id)) {
        dir = Dir((dir+1)%4);
        npos = pos + dir;
        if (cell_thread(npos, id)) {
          dir = Dir((dir+1)%4);
          npos = pos + dir;
        }
        ork_paths[id] = stack<point>();
        execute(Command(id, dir));
        should_be[id] = npos;
      } else {
        ork_paths[id] = stack<point>();
        execute(Command(id, dir));
        should_be[id] = npos;
      }
    } else if (pos_ok(npos)) {
      execute(Command(id, dir));
      should_be[id] = npos;
      ork_paths[id].pop();
    }
  }

  bool dks_con(const vector< vector<bool> >& vis, VVD& costs, double last_cost, double& new_cost, int x, int y, int size) {
    new_cost = last_cost;
    if (cell(x, y).type == GRASS) new_cost += cost_grass();
    else if (cell(x, y).type == FOREST) new_cost += cost_forest();
    else if (cell(x, y).type == SAND) new_cost += cost_sand();
    else if (cell(x, y).type == CITY) new_cost += 0.01;
    // else if (cell(x, y).type == PATH) new_cost += 0.2;
    bool con = cell(x, y).type != WATER;
    if (size <= 4) {
      if (cell(x, y).unit_id != -1) con = con and not is_my_ork(cell(x, y).unit_id);
      con = con and not want_to_go(Pos(x, y));
    }
    con = con and (not vis[x][y] or new_cost < costs[x][y]);
    return con;
  }

  bool bfs_found_con(const dks_cons& con, int x, int y) {
    bool cond;
    if (con.attacker != -1) { // ATACAR
      cond = cell(x, y).unit_id == con.attacker;
    } else if (con.any_city) { // ANY CITY
      if (cell(x, y).type == CITY) {
        if (con.mine) cond = city_owner(cell(x, y).city_id) == me();
        else cond = city_owner(cell(x, y).city_id) != me();
      } else return false;
    } else if (con.a_path) { // SPECIFIED PATH
      if (cell(x, y).type == PATH) cond = cell(x, y).path_id == con.id;
      else return false;
    } else { // NOT ANY CITY
      cond = cell(x, y).city_id == con.id;
      // if (con.mine) cond = cond and city_owner(cell(x, y).city_id) == me();
      // else cond = cond and city_owner(cell(x, y).city_id) != me();
    }

    return cond;
  }

  // Finds an optimal path to a given city or path
  void dijkstra(vector<point>& v, vector< vector<bool> >& vis, point& b, const dks_cons& con) {
    priority_queue< pair<double, point>, vector< pair<double, point> >, greater<pair<double, point>> > camins;
    VVD costs(rows(), VD(cols(), 29999));
    camins.push(make_pair(0, v[0]));
    while (not camins.empty()) {
      point p = camins.top().second;
      camins.pop();
      if (bfs_found_con(con, p.x, p.y)) {
          b = p;
          return;
      } else {
        vis[p.x][p.y] = true;
        double cost = costs[p.x][p.y];
        double new_cost;
        int size = v.size();
        if (p.x-1 > 0 and dks_con(vis, costs, cost, new_cost, p.x-1, p.y, size)) {
          point po = point(p.x-1, p.y, p.x, p.y);
          camins.push(make_pair(new_cost, po));
          v.push_back(po);
          vis[p.x-1][p.y] = true;
          costs[p.x-1][p.y] = new_cost;
        }
        if (p.x+1 < rows() and dks_con(vis, costs, cost, new_cost, p.x+1, p.y, size)) {
          point po = point(p.x+1, p.y, p.x, p.y);
          camins.push(make_pair(new_cost, po));
          v.push_back(po);
          vis[p.x+1][p.y] = true;
          costs[p.x+1][p.y] = new_cost;
        }
        if (p.y-1 > 0 and dks_con(vis, costs, cost, new_cost, p.x, p.y-1, size)) {
          point po = point(p.x, p.y-1, p.x, p.y);
          camins.push(make_pair(new_cost, po));
          v.push_back(po);
          vis[p.x][p.y-1] = true;
          costs[p.x][p.y-1] = new_cost;
        }
        if (p.y+1 < cols() and dks_con(vis, costs, cost, new_cost, p.x, p.y+1, size)) {
          point po = point(p.x, p.y+1, p.x, p.y);
          camins.push(make_pair(new_cost, po));
          v.push_back(po);
          vis[p.x][p.y+1] = true;
          costs[p.x][p.y+1] = new_cost;
        }
      }
    }
  }

  void dks_to_path(const vector<point>& v, stack<point>& s, point& P) {
    s.push(P);
    for (int i=v.size()-1; i>=1; --i) {
        if (v[i].x == P.xa and v[i].y == P.ya) {
          P = v[i];
          s.push(P);
        }
    }
  }

  void calcular_cami_dks(int x, int y, stack<point>& cami, const dks_cons& con) {
    vector<point> VP;
    point P;
    vector< vector<bool> > VVB(rows(), vector<bool>(cols(), false));
    P = point(x, y, -1, -1);
    VP.push_back(P);
    dijkstra(VP, VVB, P, con);
    dks_to_path(VP, cami, P);
  }

  void check_enemy(point& p, int x, int y, int my_health) {
    Cell cel = cell(x, y);
    if (cel.unit_id != -1 and not is_my_ork(cel.unit_id) and unit(cel.unit_id).health < my_health) p = point(x, y, -1, -1);
  }

  point check_enemies(const Pos& p, int id) {
    point go_to = point(-1, -1, -1, -1);
    int my_health = unit(id).health;

    check_enemy(go_to, p.i-1, p.j, my_health);
    check_enemy(go_to, p.i+1, p.j, my_health);
    check_enemy(go_to, p.i, p.j-1, my_health);
    check_enemy(go_to, p.i, p.j+1, my_health);

    return go_to;
  }

  void update_map() {
    orks_in_city = VVI(nb_cities(), VI(0));
    orks_in_paths = VVI(nb_paths(), VI(0));
    ids = orks(me());
    for (int i=0; i<rows(); ++i) {
      for (int j=0; j<cols(); ++j) {
        int ork_id = cell(i, j).unit_id;
        int city_id = cell(i, j).city_id;
        int path_id = cell(i, j).path_id;
        ork_at[i][j] = ork_id;
        if (city_id != -1) {
          cities[city_id] = city_owner(city_id);
          if (cell(i, j).unit_id != -1) orks_in_city[city_id].push_back(cell(i, j).unit_id);
        }
        if (path_id != -1) {
          paths[path_id] = path_owner(path_id);
          if (cell(i, j).unit_id != -1) orks_in_paths[path_id].push_back(cell(i, j).unit_id);
        }
        for (unsigned int k = 0; k<ids.size(); ++k) if (ork_id == ids[k]) my_ork_at[ids[k]] = Pos(i, j);
      }
    }
  }

  void check_enemies_and_move(int id) {
    point go_to = check_enemies(my_ork_at[id], id);
    if (go_to.x != -1) {
      ork_paths[id] = stack<point>();
      ork_paths[id].push(go_to);
      move(id, ork_paths[id].top());
    } else {
      if (my_ork_at[id] != should_be[id] or ork_paths[id].empty()) {
        ork_paths[id] = stack<point>();
        calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
      }
      move(id, ork_paths[id].top());
    }
  }

  bool want_to_go(const Pos& p) {
    for (unsigned int i=0; i<ids.size(); ++i) {
      if (not ork_paths[ids[i]].empty() and ork_paths[ids[i]].top().x == p.i and ork_paths[ids[i]].top().y == p.j) return true;
    }
    return false;
  }

  bool surrounded_by_cities(const Pos& p) {
    for (int i=0; i<4; ++i) {
      Pos np = p + Dir(i);
      if (cell(np.i, np.j).type != CITY) return false; 
    }
    return true;
  }

  bool can_leve_city(int city_id) {
    for (unsigned int i=0; i<orks_in_city[city_id].size(); ++i) {
      if (not is_my_ork(orks_in_city[city_id][i])) return false;
    }
    return true;
  }

  bool not_my_paths(int city_id, int& path_to_go) {
    City c = city(city_id);
    for (unsigned int i=0; i<c.size(); ++i) {
      if (can_leve_city(city_id)) {
        Cell path = cell(c[i].i+1, c[i].j);
        if (path.type == PATH and path_owner(path.path_id) != me() and orks_in_paths[path.path_id].size() == 0) {
          path_to_go = path.path_id;
          return true;
        }

        path = cell(c[i].i-1, c[i].j);
        if (path.type == PATH and path_owner(path.path_id) != me() and orks_in_paths[path.path_id].size() == 0) {
          path_to_go = path.path_id;
          return true;
        }

        path = cell(c[i].i, c[i].j+1);
        if (path.type == PATH and path_owner(path.path_id) != me() and orks_in_paths[path.path_id].size() == 0) {
          path_to_go = path.path_id;
          return true;
        }

        path = cell(c[i].i, c[i].j-1);
        if (path.type == PATH and path_owner(path.path_id) != me() and orks_in_paths[path.path_id].size() == 0) {
          path_to_go = path.path_id;
          return true;
        }
      }
    }
    return false;
  }

  void defensar(int id, int city_id) {
    int path_to_go = 0;
    if (not_my_paths(city_id, path_to_go)) {
      orders[id].first = 3;
      ork_paths[id] = stack<point>();
      calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(false, true, false, path_to_go, -1));
      move(id, ork_paths[id].top());
      return;
    }
    
    for (int i=0; i<4; ++i) {
      Pos p = Pos(my_ork_at[id].i, my_ork_at[id].j) + Dir(i);
      if (cell(p.i, p.j).city_id == city_id and cell(p.i, p.j).unit_id == -1 and not want_to_go(p) and surrounded_by_cities(p)) {
        execute(Command(id, Dir(i)));
        return;
      }
    }
    for (int i=0; i<4; ++i) {
      Pos p = Pos(my_ork_at[id].i, my_ork_at[id].j) + Dir(i);
      if (cell(p.i, p.j).city_id == city_id and cell(p.i, p.j).unit_id == -1 and not want_to_go(p)) {
        execute(Command(id, Dir(i)));
        return;
      }
    }
  }

  void cardinal(int id, int order_id, int city_id) {
    if (order_id == -1) { // NO TE ORDRE

      orders[id].first = 0;
      ork_paths[id] = stack<point>();
      calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
      move(id, ork_paths[id].top());

    } else if (order_id == 0) { // ESTA ANANT A LA CIUTAT MES PROPERA

      if (city_id != -1) {
        int guardia = defensant[city_id];
        if (guardia == -1) {
          orders[id].first = 1;
          orders[id].second = city_id;
          defensar(id, city_id);
          defensant[city_id] = id;
        } else if (is_my_ork(guardia)) {

          if (cell(my_ork_at[guardia].i, my_ork_at[guardia].j).city_id == city_id) {
            if (unit(guardia).health < unit(id).health) {
              orders[id].first = 1;
              orders[id].second = city_id;
              defensar(id, city_id);
              defensant[city_id] = id;
              orders[guardia].first = 0;
            } else {
              check_enemies_and_move(id);
            }
          } else {
            orders[id].first = 1;
            orders[id].second = city_id;
            defensar(id, city_id);
            defensant[city_id] = id;
            orders[guardia].first = 0;
          }

        } else {
          orders[id].first = 1;
          orders[id].second = city_id;
          defensar(id, city_id);
          defensant[city_id] = id;
          orders[guardia].first = 0;
        }
      } else check_enemies_and_move(id);

    } else if (order_id == 1) { // ESTA DEFENSANT LA CIUTAT

      if (city_id != -1 and cell(my_ork_at[id].i, my_ork_at[id].j).city_id == city_id) {
        VI inside = orks_in_city[city_id];
        if (inside.size() > 1) {
          bool attack = false;
          int attacker;
          for (unsigned int i=0; i<inside.size() and not attack; ++i) {
            if (not is_my_ork(inside[i]) and unit(inside[i]).health < unit(id).health) {
              attack = true;
              attacker = inside[i];
            }
          }
          if (attack) {
            ork_paths[id] = stack<point>();
            calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, attacker));
            move(id, ork_paths[id].top());
            orders[id].first = 2;
            orders[id].second = attacker;
          } else defensar(id, orders[id].second);
        } else defensar(id, orders[id].second);
      } else {
        ork_paths[id] = stack<point>();
        calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
        move(id, ork_paths[id].top());
        orders[id].first = 0;
      }

    } else if (order_id == 2) { // ESTA ATACANT A ALGU

      if (city_id != -1 and cell(my_ork_at[id].i, my_ork_at[id].j).city_id == city_id) {
        VI inside = orks_in_city[city_id];
        bool still = false;
        for (unsigned int i=0; i<inside.size() and not still; ++i) if (inside[i] == orders[id].second) still = true;
        if (still) {
          ork_paths[id] = stack<point>();
          calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, orders[id].second));
          move(id, ork_paths[id].top());
        } else {
          orders[id].first = 1;
          orders[id].second = city_id;
          defensar(id, city_id);
        }
      } else {
        ork_paths[id] = stack<point>();
        calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
        move(id, ork_paths[id].top());
        orders[id].first = 0;
      }

    } else if (order_id == 3) { // ESTA RECONQUERINT UN PATH
      if (city_id != -1 and cell(my_ork_at[id].i, my_ork_at[id].j).city_id == city_id) {
        VI inside = orks_in_city[city_id];
        if (inside.size() > 1) {
          bool attack = false;
          int attacker;
          for (unsigned int i=0; i<inside.size() and not attack; ++i) {
            if (not is_my_ork(inside[i]) and unit(inside[i]).health < unit(id).health) {
              attack = true;
              attacker = inside[i];
            }
          }
          if (attack) {
            ork_paths[id] = stack<point>();
            calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, attacker));
            move(id, ork_paths[id].top());
            orders[id].first = 2;
            orders[id].second = attacker;
          } else {
            if (not ork_paths[id].empty()) move(id, ork_paths[id].top());
            else defensar(id, city_id);
          }
        } else {
          if (not ork_paths[id].empty()) move(id, ork_paths[id].top());
          else defensar(id, city_id);
        }
      } else if (cell(my_ork_at[id].i, my_ork_at[id].j).path_id != -1) {
        if (cell(my_ork_at[id].i, my_ork_at[id].j).path_id != -1) {
            ork_paths[id] = stack<point>();
          calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(false, false, false, orders[id].second, -1));
          orders[id].first = 1;
          move(id, ork_paths[id].top());
        }
      } else {
        ork_paths[id] = stack<point>();
        calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
        move(id, ork_paths[id].top());
        orders[id].first = 0;
      }

    }
  }

  virtual void play () {
    
    if (round() == 0) {
      cities = VI(nb_cities());
      paths = VI(nb_paths());
      orks_in_city = VVI(nb_cities(), VI(0));
      orks_in_paths = VVI(nb_paths(), VI(0));
      ork_at = VVI(rows(), VI(cols(), -1));
      ids = orks(me());
      my_ork_at = VP(nb_orks()*4);
      ork_paths = vector< stack<point> >(nb_orks()*4);
      orders = vector<pair<int, int>>(nb_orks()*4, {-1, -1});
      should_be = VP(nb_orks()*4, Pos(-1, -1));
      defensant = VI(nb_cities(), -1);
      update_map();

      for (unsigned int c=0; c<ids.size(); ++c) {
        int id = ids[c];
        calcular_cami_dks(my_ork_at[id].i, my_ork_at[id].j, ork_paths[id], dks_cons(true, false, false, -1, -1));
        move(id, ork_paths[id].top());
      }
    } else {
      update_map();
      for (unsigned int c=0; c<ids.size(); ++c) {
        int id = ids[c];
        int order_id = orders[id].first;
        int city_id = cell(my_ork_at[id].i, my_ork_at[id].j).city_id;
        cardinal(id, order_id, city_id);
      }
    }

  }

};

RegisterPlayer(PLAYER_NAME);