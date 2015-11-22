#ifndef __PHG4TRUTHINFOCONTAINER_H__
#define __PHG4TRUTHINFOCONTAINER_H__

#include <phool/PHObject.h>
#include <map>
#include <set>

class PHG4Particle;
class PHG4VtxPoint;

class PHG4TruthInfoContainer: public PHObject {
  
public:

  typedef std::map<int,PHG4Particle *> Map;
  typedef Map::iterator Iterator;
  typedef Map::const_iterator ConstIterator;
  typedef std::pair<Iterator, Iterator> Range;
  typedef std::pair<ConstIterator, ConstIterator> ConstRange;

  typedef std::map<int,PHG4VtxPoint *> VtxMap;
  typedef VtxMap::iterator VtxIterator;
  typedef VtxMap::const_iterator ConstVtxIterator;
  typedef std::pair<VtxIterator, VtxIterator> VtxRange;
  typedef std::pair<ConstVtxIterator, ConstVtxIterator> ConstVtxRange;

  PHG4TruthInfoContainer();
  virtual ~PHG4TruthInfoContainer();

  void Reset();
  void identify(std::ostream& os = std::cout) const;

  // --- particle storage ------------------------------------------------------
 
  //! Add a particle that the user has created
  ConstIterator AddParticle(const int particleid, PHG4Particle* newparticle);
  void delete_particle(Iterator piter);
  
  PHG4Particle* GetParticle(const int particleid);
  PHG4Particle* GetPrimaryParticle(const int particleid);

  //! Get a range of iterators covering the entire container
  Range GetParticleRange() {return Range(particlemap.begin(),particlemap.end());}
  ConstRange GetParticleRange() const {return ConstRange(particlemap.begin(),particlemap.end());}

  Range GetPrimaryParticleRange() {return Range(particlemap.upper_bound(0),particlemap.end());}
  ConstRange GetPrimaryParticleRange() const {return ConstRange(particlemap.upper_bound(0),particlemap.end());}

  Range GetSecondaryParticleRange() {return Range(particlemap.begin(),particlemap.upper_bound(0));}
  ConstRange GetSecondaryParticleRange() const {return ConstRange(particlemap.begin(),particlemap.upper_bound(0));}
  
  //! particle size
  unsigned int size( void ) const {return particlemap.size();}
  int GetNumPrimaryVertexParticles() {
    return std::distance(particlemap.upper_bound(0),particlemap.end());
  }
  
  //! Get the Particle Map storage
  const Map& GetMap() const {return particlemap;}
  
  int maxtrkindex() const;
  int mintrkindex() const;

  std::pair< std::map<int,int>::const_iterator,
	     std::map<int,int>::const_iterator > GetEmbeddedTrkIds() const {
    return std::make_pair(particle_embed_flags.begin(), particle_embed_flags.end());
  }
  void AddEmbededTrkId(const int id, const int flag) {
    particle_embed_flags.insert(std::make_pair(id,flag));
  }

  int isEmbeded(const int trackid) const;
   
  // --- vertex storage --------------------------------------------------------
  
  //! Add a vertex and return an iterator to the user
  ConstVtxIterator AddVertex(const int vtxid, PHG4VtxPoint* vertex);
  void delete_vtx(VtxIterator viter);
  
  PHG4VtxPoint* GetVtx(const int vtxid);
  PHG4VtxPoint* GetPrimaryVtx(const int vtxid);
  
  //! Get a range of iterators covering the entire vertex container
  VtxRange GetVtxRange() {return VtxRange(vtxmap.begin(),vtxmap.end());}
  ConstVtxRange GetVtxRange() const {return ConstVtxRange(vtxmap.begin(),vtxmap.end());}

  VtxRange GetPrimaryVtxRange() {return VtxRange(vtxmap.upper_bound(0),vtxmap.end());}
  ConstVtxRange GetPrimaryVtxRange() const {return ConstVtxRange(vtxmap.upper_bound(0),vtxmap.end());}

  VtxRange GetSecondaryVtxRange() {return VtxRange(vtxmap.begin(),vtxmap.upper_bound(0));}
  ConstVtxRange GetSecondaryVtxRange() const {return ConstVtxRange(vtxmap.begin(),vtxmap.upper_bound(0));}
  
  //! Get the number of vertices stored
  unsigned int GetNumVertices() const {return vtxmap.size();}

  //! Get the Vertex Map storage
  const VtxMap& GetVtxMap() const {return vtxmap;}

  int maxvtxindex() const;
  int minvtxindex() const;

  // returns the first primary vertex that was processed by Geant4
  int GetPrimaryVertexIndex() {return (vtxmap.lower_bound(1))->first;}

  std::pair< std::map<int,int>::const_iterator,
	     std::map<int,int>::const_iterator > GetEmbeddedVtxIds() const {
    return std::make_pair(vertex_embed_flags.begin(), vertex_embed_flags.end());
  }
  void AddEmbededVtxId(const int id, const int flag) {
    vertex_embed_flags.insert(std::make_pair(id,flag));
  }

  int isEmbededVtx(const int vtxid) const;
  
  // deprecated interface, confusingly named as we store particles not hits ----
  // do not call these functions in new code, i'm leaving these for now for
  // build compatibility outside of coresoftware
  
  ConstIterator AddHit(const int particleid, PHG4Particle *newparticle) {return AddParticle(particleid,newparticle);}
  PHG4Particle* GetHit(const int particleid) {return GetParticle(particleid);}
  PHG4Particle* GetPrimaryHit(const int particleid) {return GetPrimaryParticle(particleid);}
  Range GetHitRange() {return GetParticleRange();}
  ConstRange GetHitRange() const {return GetParticleRange();}
  void delete_hit(Iterator piter) {delete_particle(piter);}

 private:

  // particle storage map format description:
  // primary particles are appended in the positive direction
  // secondary particles are appended in the negative direction
  // subevent boundaries between geant runs are stored in the subevent markers

  // +N   primary particle id => particle*
  // +N-1 
  // ...
  // +1   primary particle id => particle*
  // 0    no entry
  // -1   secondary particle id => particle*
  // ...
  // -M+1
  // -M   secondary particle id => particle*
  
  Map particlemap; //< particle => particle*

  // vertex storage map format is similar to the above particle map
  VtxMap vtxmap; //< vertex id => vertex*

  // embed flag storage, will typically be set for only a few entries or none at all
  std::map< int, int> particle_embed_flags; //< trackid => embed flag
  std::map< int, int> vertex_embed_flags;   //< vtxid => embed flag

  ClassDef(PHG4TruthInfoContainer,1)
};

#endif
