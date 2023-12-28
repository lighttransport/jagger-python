// Jagger -- deterministic pattern-based Japanese tagger
//  $Id: train_jagger.cc 2031 2023-02-17 21:47:05Z ynaga $
// Copyright (c) 2022 Naoki Yoshinaga <ynaga@iis.u-tokyo.ac.jp>
#include <jagger.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static const char* chars_[] = {"０１２３４５６７８９〇一二三四五六七八九十百千万億兆数・", "ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ＠：／．", "ァアィイゥウェエォオカガキギクグケゲコゴサザシジスズセゼソゾタダチヂッツヅテデトドナニヌネノハバパヒビピフブプヘベペホボポマミムメモャヤュユョヨラリルレロヮワヰヱヲンヴヵヶヷヸヹヺーヽヾヿ", 0};

struct triple {
  int first, second, third;
  triple (const int first_, const int second_, const int third_) : first (first_), second (second_), third (third_) {}
};

int main (int argc, char** argv) {
  std::string train, dict;
  { // options (minimal)
    if (argc < 3) {
      fprintf(stderr, "Usage: %s dict train\n", argv[0]);
      exit(-1);
    }
    dict = argv[1];
    train = argv[2];
    //extern char *optarg;
    //extern int optind;
    //for (int opt = 0; (opt = getopt (argc, argv, "d:")) != -1; )
    //  if (opt == 'd') dict  = optarg;
    //if (optind == argc) errx (1, " extract patterns for Jagger from dictionary and training data\nUsage: %s -d dict train > patterns\n\nOptions:\n -d dict\tdictionary csv", argv[0]);
    //train = argv[optind];
  }
  ccedar::da <char, int> chars;
  sbag_t fbag, pbag_;
  std::vector <std::map <int, int> > si2fi2fi;
  std::vector <int> si2fi;
  std::vector <std::map <int, std::pair <int, int> > > pi2fi2sc;
  std::vector <int> fi2c;
  size_t max_plen = 0;
  char* line = 0;
  std::fprintf (stderr, "reading seed patterns from dictionary...");
  { // read seeds from dictionary
    simple_reader reader (dict.c_str ());
    while (const size_t len = reader.gets (&line)) {
      const char *p (line), *seed (p), *end (p + len - 1);
      const bool quoted = *p++ == '"';
      if (quoted)
        while (*p != '"') ++p; // for words including ,
      p = skip_to (p, 1, ',');
      max_plen = std::max (static_cast <size_t> (p - seed - (quoted ? 3 : 1)), max_plen);
      const int pi = pbag_.to_i (quoted ? seed + 1 : seed, p - seed - (quoted ? 3 : 1));
      if (pi == si2fi2fi.size ()) si2fi2fi.push_back (std::map <int, int> ());
      const char *f = skip_to (p, 3, ','); // read features
      p = skip_to (f, NUM_POS_FIELD, ',') - 1;
      si2fi2fi[pi].insert (std::make_pair (fbag.to_i (f, p - f),
                                           fbag.to_i (f, end - f))); // may not unique
    }
    fi2c.resize (fbag.size (), 0);
  }
  std::fprintf (stderr, "done; %zu words, %zu features\n", si2fi2fi.size (), fbag.size ());
  std::fprintf (stderr, "regarding num / alpha / kana as seed patterns...");
  for (int i (0), b (0); chars_[i]; ++i) // read seeds from num / alpha / kana
    for (const char *p = &chars_[i][0]; *p; p += b) {
      chars.update (p, b = u8_len (p)) =  i;
      pbag_.to_i (p, b);
    }
  pi2fi2sc.resize (pbag_.size ());
  const int num_seed = static_cast <int> (pbag_.size ());
  std::fprintf (stderr, "done; # seeds = %d\n", num_seed);
  { // enumerate patterns
    std::fprintf (stderr, "mining patterns from training data...");
    std::vector <triple> tokens, pis;
    std::string sent;
    simple_reader reader (train.c_str ());
    while (const size_t len = reader.gets (&line)) {
      if (std::strncmp (line, "EOS\n", 4) == 0) {
        char *p (&sent[0]), *end (&sent[0] + sent.size ());
        std::string f_prev ("\tBOS");
        for (std::vector <triple>::const_iterator it = tokens.begin (); it != tokens.end (); ++it, pis.clear ()) {
          const int tlen (it->first), fi (it->second), fi_ (it->third);
          for (char *q = p + tlen; q <= std::min (p + max_plen, end); q += u8_len (q)) {
            pis.push_back (triple (pbag_.to_i (p, q - p), fi, tlen));
            const bool first = pis.back ().first >= pi2fi2sc.size ();
            pis.push_back (triple (pbag_.to_i (std::string (p, q - p) + f_prev), fi, tlen));
            if (first) break; // new pattern
          }
          const int n_ = pbag_.find (p, tlen); // reject tokens > max_plen
          if ((n_ == -1 || n_ > num_seed) && char_type (p, p + tlen, chars) != 0) { // POS-only pattern for unseen tokens
            if (fi2c.size () <= fi_) fi2c.resize (fi + 1);
            fi2c[fi_] += 1;
            const int fi__ = fbag.to_i (fbag.to_s (fi_) + ",*.*,*");
            pis.push_back (triple (pbag_.to_i (f_prev), fi__, 0));
          }
          pi2fi2sc.resize (pbag_.size ());
          for (std::vector <triple>::const_iterator jt = pis.begin (); jt != pis.end (); ++jt)
            ++pi2fi2sc[jt->first].insert (std::make_pair (jt->second, std::make_pair (jt->third, 0))).first->second.second;
          f_prev = "\t" + fbag.to_s (fi_);
          p += tlen;
        }
        tokens.clear ();
        sent.clear ();
      } else { // token
        const char *t (line), *f (skip_to (t, 1, '\t')), *p (skip_to (f, NUM_POS_FIELD, ',') - 1), *end (line + len - 1);
        tokens.push_back (triple (f - 1 - t, fbag.to_i (f, end - f), fbag.to_i (f, p - f)));
        sent += std::string (t, f - 1 - t);
      }
    }
  }
  std::fprintf (stderr, "done; %zu pattern candidates\n", pbag_.size ());
  std::map <int, std::pair <int, int> > pi2sf;
  ccedar::da <char, int> patterns;
  std::vector <std::pair <size_t, int> > counter;
  std::vector <std::pair <std::string, int> > pis;
  { // pruning patterns
    long max_fi = std::max_element (fi2c.begin (), fi2c.end ()) - fi2c.begin ();
    for (int i = 0; i < pi2fi2sc.size (); ++i)
      pis.push_back (std::make_pair (pbag_.to_s (i), i));
    std::sort (pis.begin (), pis.end ());
    std::fprintf (stderr, "pruning patterns...");
    for (int i = 0; i < pis.size (); ++i) {
      const int pi = pis[i].second;
      const std::string& p = pbag_.to_s (pi);
      int bytes (p.size ()), fi (max_fi), count (0);
      if (pi2fi2sc[pi].empty ()) { // unseen patterns (seeds)
        if (pi < si2fi2fi.size ()) { // words in dictionary
          const std::map <int, int>& fi2fi = si2fi2fi[pi];
          std::map <int, int>::const_iterator jt (fi2fi.begin ()), jt_end (fi2fi.end ());
          size_t max_fic (fi2c[jt->first]), fi_ (jt->first);
          for (++jt; jt != jt_end; ++jt)
            if (fi2c[jt->first] > max_fic || (fi2c[jt->first] == max_fic))
              fi_ = jt->first, max_fic = fi2c[fi_];
          fi = fi2fi.find (fi_)->second;
        }
      } else { // perform pruning for seen patterns
        const std::map <int, std::pair <int, int> >& fi2sc = pi2fi2sc[pi];
        std::vector <int> s2c (max_plen + 1, 0);
        for (std::map <int, std::pair <int, int> >::const_iterator jt = fi2sc.begin ();
             jt != fi2sc.end (); ++jt) // bytes to count for pi
          s2c[jt->second.first] += jt->second.second,
                          count += jt->second.second;
        size_t max_count = 0;
        for (std::vector <int>::iterator it = s2c.begin (); it != s2c.end (); ++it)
          if (*it >= max_count) // =: prefer longer match
            max_count = *it,
                bytes = std::distance (s2c.begin (), it);
        size_t max_sfc = 0;
        for (std::map <int, std::pair <int, int> >::const_iterator jt = fi2sc.begin ();
             jt != fi2sc.end (); ++jt)
          if (jt->second.first == bytes && jt->second.second > max_sfc)
              fi = jt->first, max_sfc = jt->second.second;
        ccedar::da <char, int>::result_type result[MAX_PLEN];
        const int num = patterns.commonPrefixSearch (p.c_str (), &result[0], max_plen, p.size ());
        if (num > 0 && std::make_pair (bytes, fi) == pi2sf[result[num - 1]]) // && count < 70)
          continue;
      }
      counter.push_back (std::make_pair (count, -i));
      pi2sf.insert (std::make_pair (pi, std::make_pair (bytes, fi)));
      patterns.update (p.c_str (), p.size ()) = static_cast <int> (pi);
    }
    std::fprintf (stderr, "done; %zu -> %zu patterns\n", pi2fi2sc.size (), pi2sf.size ());
  }
  { // output patterns from frequent one to rare one
    std::sort (counter.rbegin (), counter.rend ());
    for (std::vector <std::pair <size_t, int> >::const_iterator it = counter.begin ();
         it != counter.end (); ++it) {
      const size_t pi (pis[-it->second].second), count (it->first), bytes (pi2sf[pi].first);
      const std::string &w (pbag_.to_s (pi)), &f (fbag.to_s (pi2sf[pi].second));
      const int ctype = bytes ? char_type (&w[0], &w[0] + bytes, chars) : 0;
      std::fprintf (stdout, "%zu\t%s\t%s%zu\t%d\t%s\n", count, w.c_str (), w.find ("\t") == std::string::npos ? "\t" : "", bytes, ctype, f.c_str ());
    }
  }
}
