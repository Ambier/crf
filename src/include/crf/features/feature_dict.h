namespace NLP {
  namespace CRF {

    class FeatureDict {
      public:
        FeatureDict(void) { }
        virtual ~FeatureDict(void) { };

        virtual Attribute &load(const Type &type, std::istream &in) = 0;
    };

    class WordDict : public FeatureDict {
      public:
        WordDict(const Lexicon lexicon, const size_t nbuckets=HT::MEDIUM,
        const size_t pool_size=HT::LARGE);
        WordDict(const WordDict &other);
        virtual ~WordDict(void);

        virtual Attribute &load(const Type &type, std::istream &in);
        Attribute get(const Type &type, const Raw &raw);
        Attribute &insert(const Type &type, const Raw &raw);

        void print_stats(std::ostream &out);

      private:
        class Impl;
        Impl *_impl;
    };

    class BiWordDict : public FeatureDict {
      public:
        BiWordDict(const Lexicon lexicon, const size_t nbuckets=HT::MEDIUM,
        const size_t pool_size=HT::LARGE);
        BiWordDict(const BiWordDict &other);
        virtual ~BiWordDict(void);

        virtual Attribute &load(const Type &type, std::istream &in);
        Attribute get(const Type &type, const Raw &raw1, const Raw &raw2);
        Attribute &insert(const Type &type, const Raw &raw1, const Raw &raw2);

      private:
        class Impl;
        Impl *_impl;
    };

    class TagDict : public FeatureDict {
      public:
        TagDict(const TagSet tags)
          : tags(tags), attributes() { }
        virtual ~TagDict(void) { };

        virtual Attribute &load(const Type &type, std::istream &in) {
          if (!attributes.size())
            attributes.resize(tags.size());
          Raw value;
          in >> value;
          return insert(type.name, value);
        }

        Attribute get(const Type &type, const Raw &raw) {
          return attributes[tags[raw]];
        }

        Attribute &insert(const std::string &type, const Raw &raw) {
          return attributes[tags[raw]];
        }

      private:
        const TagSet tags;
        std::vector<Attribute> attributes;
    };

    class BiTagDict : public FeatureDict {
      public:
        BiTagDict(const TagSet tags)
          : tags(tags), nklasses2(0), attributes() { }
        virtual ~BiTagDict(void) { };

        virtual Attribute &load(const Type &type, std::istream &in) {
          if (!attributes.size()) {
            nklasses2 = tags.size() * tags.size();
            attributes.resize(nklasses2);
          }
          Raw val1, val2;
          in >> val1 >> val2;
          return insert(type.name, val1, val2);
        }

        Attribute get(const Type &type, const Raw &raw1, const Raw &raw2) {
          return attributes[tags[raw1] * tags.size() + tags[raw2]];
        }

        Attribute &insert(const std::string &type, const Raw &raw1, const Raw &raw2) {
          return attributes[tags[raw1] * tags.size() + tags[raw2]];
        }

      private:
        const TagSet tags;
        uint64_t nklasses2;
        std::vector<Attribute> attributes;
    };
  }
}
