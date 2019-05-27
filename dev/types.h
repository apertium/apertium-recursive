#ifndef __TYPES_H__
#define __TYPES_H__

#include <map>
#include <string>
#include <vector>
#include <cstdarg> // arbitrary number of parameters

using namespace std;

struct node_pair;
struct node
{
public:
  vector<node_pair *> children;
  string token;
  bool leaf;
  map<string, string> sl; // node.sl["lem"] = "foo"
  map<string, string> tl; // node.sl["whole"] = "bar<n>"

  node(string _token)
  {
    token = _token;
    leaf = true;
  }
  
  // ... : node_pair *children
  node(string _token, int _num, ...) // arbitrary number of parameters
  { 
    va_list arguments; //store
    va_start(arguments, _num); // _num is the number of children nodes
    for(int i = 0; i < _num; i++)
    {
      children.push_back(va_arg( arguments, node_pair *));
    }
    va_end(arguments);

    token = _token;
    leaf = false;
  }

  // ... : node_pair *children
  node(string _token, vector<node_pair *> &_children) // arbitrary number of parameters
  {
    children = _children;
    token = _token;
    leaf = false;
  }
};

struct node_pair
{
public:
  node *sl;
  node *tl;

  // constructor for non-terminal nodes
  // ... : node_pair *node_pairs_in_sl_order, node_pair *node_pairs_inserted_in_tl, int tl_order_indexes
  node_pair(string _token_sl, int _num_sl, string _token_tl, int _num_tl, ...)
  {
    int num_inserted = (_num_tl > _num_sl) ? (_num_tl - _num_sl) : 0;

    va_list arguments;
    va_start(arguments, _num_tl); // _num_tl - last named argument

    // node_pairs_in_sl_order
    vector<node_pair *> children_sl, children_tl;
    for(int i = 0; i < _num_sl; i++)
    {
      node_pair *np = va_arg( arguments, node_pair *);
      if (np->sl != NULL) children_sl.push_back(np);
      if (np->tl != NULL) children_tl.push_back(np);
    }
    // node_pairs_inserted_in_tl
    for(int i = 0; i < num_inserted; i++)
    {
      node_pair *np = va_arg( arguments, node_pair *);
      if (np->tl != NULL) children_tl.push_back(np);
    }
    // tl_order_indexes
    vector<node_pair *> children_tl_new;
    for(int i = 0; i < _num_tl; i++) // children_tl.size()
    {
      children_tl_new.push_back(children_tl[va_arg( arguments, int ) - 1]);
    }

    va_end(arguments);

    sl = new node(_token_sl, children_sl);
    tl = new node(_token_tl, children_tl_new);
  }

  // constructor for non-terminal nodes
  node_pair(node *_sl, node *_tl)
  {
    sl = _sl;
    tl = _tl;
  }

  // constructor for terminal nodes
  node_pair(string _name_sl, string _value_sl, string _name_tl, string _value_tl)
  {
    node_pair *terminal = new node_pair(new node(_value_sl), new node(_value_tl));
    sl = new node(_name_sl, 1, terminal);
    tl = new node(_name_tl, 1, terminal);
  }

  // constructor for terminal nodes
  // string _side : "sl", "tl"
  node_pair(string _name, string _value, string _side)
  {
    if (_side == "sl")
    {
      sl = new node(_name, 1, new node_pair(new node(_value), NULL));
      tl = NULL;
    }
    else if (_side == "tl")
    {
      sl = NULL;
      tl = new node(_name, 1, new node_pair(NULL, new node(_value)));
    }
  }
};


#endif /* __TYPES_H__ */
