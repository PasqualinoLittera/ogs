/*!
  \file NodePartitionedMeshReader.h
  \author Wenqing Wang
  \date   2014.08
  \brief  Declare a class to read node-wise partitioned mesh with MPI functions.

  \copyright
  Copyright (c) 2012-2014, OpenGeoSys Community (http://www.opengeosys.org)
             Distributed under a Modified BSD License.
               See accompanying file LICENSE.txt or
               http://www.opengeosys.org/project/license
*/

#ifndef NODE_PARTITIONED_MESH_READER_H
#define NODE_PARTITIONED_MESH_READER_H

#include <fstream>
#include <string>
#include <vector>

#include <mpi.h>

#include "MeshLib/NodePartitionedMesh.h"

namespace MeshLib
{
class Node;
class Element;
}

namespace FileIO
{
/// Class for parallel reading of ascii or binary partitioned mesh files into a
/// NodePartitionedMesh via MPI.
class NodePartitionedMeshReader
{
    public:
        ///  \param comm   MPI communicator.
        NodePartitionedMeshReader(MPI_Comm comm);

        ~NodePartitionedMeshReader();

        /*!
             \brief Create a NodePartitionedMesh object, read data to it,
                    and return a pointer to it. Data files are either in
                    ASCII format or binary format.
             \param file_name_base  Name of file to be read, and it must be base name without name extension.
             \return                Pointer to Mesh object. If the creation of mesh object
                                    fails, return a null pointer.
        */
        MeshLib::NodePartitionedMesh* read(const std::string &file_name_base);

    private:
        /// Pointer to MPI commumicator;
        MPI_Comm _mpi_comm = MPI_COMM_WORLD;

        /// Number of processes in the communicator: _mpi_comm
        int _mpi_comm_size;

        /// Rank of compute core
        int _mpi_rank;

        /// MPI data type for struct NodeData.
        MPI_Datatype _mpi_node_type;

        /// Node data only for parallel reading.
        struct NodeData
        {
            std::size_t index; ///< Global node index.
            double x;
            double y;
            double z;
        };

        /// Define MPI data type for NodeData struct.
        void registerNodeDataMpiType();

        /// A collection of integers that congfigure the partitioned mesh data.
        struct PartitionedMeshInfo
        {
            long nodes;               ///< 0:    Number of all nodes of a partition,
            long base_nodes;          ///< 1:    Number of nodes for linear elements of a parition,
            long regular_elements;    ///< 2:    Number of non-ghost elements of a partition,
            long ghost_elements;      ///< 3:    Number of ghost element of a partition,
            long active_base_nodes;   ///< 4:    Number of active nodes for linear element of a parition,
            long active_nodes;        ///< 5:    Number of all active nodes a parition,
            long global_base_nodes;   ///< 6:    Number of nodes for linear element of global mesh,
            long global_nodes;        ///< 7:    Number of all nodes of global mesh,
            long offset[5];           ///< 8~12: Offsets of positions of partitions in the data arrays
                                      ///        (only 8 and 9 are used for ascii input)
            long extra_flag;          ///< 13:   Reserved for extra flag.

            std::size_t size() const { return 14; }
            long* data() { return &nodes; }

        } _mesh_info;

        /*!
            \brief Create a new mesh of NodePartitionedMesh after reading and processing the data
            \param mesh_name    Name assigned to the new mesh.
            \param mesh_nodes   Node data.
            \param glb_node_ids Global IDs of nodes.
            \param mesh_elems   Element data.
            \return         True on success and false otherwise.
        */
        MeshLib::NodePartitionedMesh* newMesh(std::string const& mesh_name,
                                              std::vector<MeshLib::Node*> const& mesh_nodes,
                                              std::vector<std::size_t> const& glb_node_ids,
                                              std::vector<MeshLib::Element*> const& mesh_elems) const;

        /*!
            \brief Parallel reading of a binary file via MPI_File_read, and it is called by readBinary
                   to read files of mesh data head, nodes, non-ghost elements and ghost elements, respectively.
            \note           In case of failure during opening of the file, an
                            error message is printed.
            \note If the number of elements in container is larger than
                  MPI_file_read() supports (maximum of current \c int type), an
                  error is printed.
            \param filename File name containing data.
            \param offset   Displacement of the data accessible from the view.
                            see MPI_File_set_view() documentation.
            \param type     Type of data.
            \param data     A container to be filled with data. Its size is used
                            to determine how many values should be read.
            \tparam DATA    A homogeneous contaner type supporting data() and size().
            \return         True on success and false otherwise.
        */
        template <typename DATA>
        bool readBinaryDataFromFile(std::string const& filename, MPI_Offset offset,
                                    MPI_Datatype type, DATA& data) const;

        /*!
             \brief Create a NodePartitionedMesh object, read binary mesh data
                    in the manner of parallel, and return a pointer to it.
                    Four binary files have to been read in this function named as:
                    file_name_base+_partitioned_msh_cfg[number of partitions].bin
                    file_name_base+_partitioned_msh_nod[number of partitions].bin
                    file_name_base+_partitioned_msh_ele[number of partitions].bin
                    file_name_base+_partitioned_msh_ele_g[number of partitions].bin
                    in which, the first file contains an array of integers for the
                    PartitionMeshInfo for all partitions

                    the second file contains a struct type (long, double double double) array of
                    nodes information of global IDs and coordinates of all partitions.

                    the third file contains a long type integer array of element information of
                    material ID, element type and node IDs of each non-ghost element of all partitoions.

                    the forth file contains a long type integer array of element information of
                    material ID, element type and node IDs of each ghost element of all partitoions.
             \param file_name_base  Name of file to be read, which must be a name with the
                               path to the file and without file extension.
             \return           Pointer to Mesh object.
        */
        MeshLib::NodePartitionedMesh* readBinary(const std::string &file_name_base);

        /*!
            \brief Open ASCII files of node partitioned mesh data.

            \param file_name_base  Name of file to be read, which must be a name with the
                                   path to the file and without file extension.
            \param is_cfg          Input stream for the file contains configuration data.
            \param is_node         Input stream for the file contains node data.
            \param is_elem         Input stream for the file contains element data.
            \return                Return true if all files are good.
        */
        bool openASCIIFiles(std::string const& file_name_base,std::ifstream& is_cfg,
                            std::ifstream& is_node, std::ifstream& is_elem) const;

        /*!
            \brief Read mesh nodes from an ASCII file and cast to the corresponding rank.

            \param is_node    Input stream for the file contains node data.
            \param part_id    Partition ID.
            \param mesh_nodes Node vector to be filled.
            \param glb_node_ids Global Node IDs to be filled.
        */
        bool readCastNodesASCII(std::ifstream& is_node, const int part_id,
                                std::vector<MeshLib::Node*> &mesh_nodes,
                                std::vector<std::size_t> &glb_node_ids) const;

        /*!
            \brief Read mesh elements from an ASCII file and cast to the corresponding rank.

            \param is_elem    Input stream for the file contains element data.
            \param part_id    Partition ID.
            \param data_size  Total size of the data to be read. This type is an
                              int because of MPI_Send() implicit cast.
            \param process_ghost Flag to process ghost element.
            \param mesh_nodes Node vector to be filled.
            \param mesh_elems Element vector to be filled.
        */
        bool readCastElemsASCII(std::ifstream& is_elem, const int part_id,
                                const long data_size, const bool process_ghost,
                                const std::vector<MeshLib::Node*> &mesh_nodes,
                                std::vector<MeshLib::Element*> &mesh_elems) const;

        /*!
             \brief Create a NodePartitionedMesh object, read ASCII mesh data,
                    and return a pointer to it.
                    Three ASCII files have to been read in this function named as:
                    file_name_base+_partitioned_cfg[number of partitions].msh
                    file_name_base+_partitioned_nodes[number of partitions].msh
                    file_name_base+_partitioned_elems[number of partitions].msh
                    in which, the first file contains an array of integers for the
                    PartitionMeshInfo for all partitions

                    the second file contains nodes information of global IDs and coordinates
                    of all partitions.

                    the third file contains element information of material ID, element type
                    and node IDs of each element of all partitoions.
             \param file_name_base  Name of file to be read, which must be a name with the
                               path to the file and without file extension.
             \return           Pointer to Mesh object.
        */
        MeshLib::NodePartitionedMesh* readASCII(const std::string &file_name_base);

        /*!
             \brief Read elements data from ASCII file.
             \param ins       Input stream.
             \param elem_data Pointer to array that contains element data, which to be filled.
             \param ghost     Flag to read ghost elements.
        */
        void readElementASCII(std::ifstream &ins,
                std::vector<long>& elem_data,
                const bool ghost = false) const;

        /*!
             \brief Set mesh nodes from a tempory array containing node data read from file.
             \param node_data  Array containing node data read from file.
             \param mesh_node  Vector of mesh nodes to be set.
             \param glb_node_ids  Global IDs of nodes of a partition.
        */
        void setNodes(const std::vector<NodeData> &node_data,
                std::vector<MeshLib::Node*> &mesh_node,
                    std::vector<std::size_t> &glb_node_ids) const;

        /*!
             \brief Set mesh elements from a tempory array containing node data read from file.
             \param mesh_nodes        Vector of mesh nodes used to set element nodes.
             \param elem_data         Array containing element data read from file.
             \param mesh_elems        Vector of mesh elements to be set.
             \param ghost             Flag of processing ghost elements.
        */
        void setElements(const std::vector<MeshLib::Node*> &mesh_nodes,
                const std::vector<long> &elem_data,
                std::vector<MeshLib::Element*> &mesh_elems,
                const bool ghost = false) const;

};

}   // FileIO

#endif  // READ_NODE_PARTITIONED_MESH_H
