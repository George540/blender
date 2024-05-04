/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

/** \file
 * \ingroup bke
 */

#include "BKE_main.hh"

#include "BLI_function_ref.hh"
#include "BLI_map.hh"

#include <string>

struct bContext;
struct BlendFileData;
struct BlendFileReadParams;
struct BlendFileReadReport;
struct BlendFileReadWMSetupData;
struct ID;
struct IDNameLib_Map;
struct Library;
struct LibraryIDLinkCallbackData;
struct MemFile;
struct ReportList;
struct UserDef;
struct WorkspaceConfigFileData;

/**
 * Check whether given path ends with a blend file compatible extension
 * (`.blend`, `.ble` or `.blend.gz`).
 *
 * \param str: The path to check.
 * \return true is this path ends with a blender file extension.
 */
bool BKE_blendfile_extension_check(const char *str);
/**
 * Try to explode given path into its 'library components'
 * (i.e. a .blend file, id type/group, and data-block itself).
 *
 * \param path: the full path to explode.
 * \param r_dir: the string that'll contain path up to blend file itself ('library' path).
 * WARNING! Must be at least #FILE_MAX_LIBEXTRA long (it also stores group and name strings)!
 * \param r_group: a pointer within `r_dir` to the 'group' part of the path, if any ('\0'
 * terminated). May be NULL.
 * \param r_name: a pointer within `r_dir` to the data-block name, if any ('\0' terminated). May be
 * NULL.
 * \return true if path contains a blend file.
 */
bool BKE_blendfile_library_path_explode(const char *path,
                                        char *r_dir,
                                        char **r_group,
                                        char **r_name);

/**
 * Check whether a given path is actually a Blender-readable, valid .blend file.
 *
 * \note Currently does attempt to open and read (part of) the given file.
 */
bool BKE_blendfile_is_readable(const char *path, ReportList *reports);

/**
 * Shared setup function that makes the data from `bfd` into the current blend file,
 * replacing the contents of #G.main.
 * This uses the bfd returned by #BKE_blendfile_read and similarly named functions.
 *
 * This is done in a separate step so the caller may perform actions after it is known the file
 * loaded correctly but before the file replaces the existing blend file contents.
 */
void BKE_blendfile_read_setup_readfile(bContext *C,
                                       BlendFileData *bfd,
                                       const BlendFileReadParams *params,
                                       BlendFileReadWMSetupData *wm_setup_data,
                                       BlendFileReadReport *reports,
                                       bool startup_update_defaults,
                                       const char *startup_app_template);

/**
 * Simpler version of #BKE_blendfile_read_setup_readfile used when reading undo steps from
 * memfile. */
void BKE_blendfile_read_setup_undo(bContext *C,
                                   BlendFileData *bfd,
                                   const BlendFileReadParams *params,
                                   BlendFileReadReport *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 */
BlendFileData *BKE_blendfile_read(const char *filepath,
                                  const BlendFileReadParams *params,
                                  BlendFileReadReport *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 */
BlendFileData *BKE_blendfile_read_from_memory(const void *file_buf,
                                              int file_buf_size,
                                              const BlendFileReadParams *params,
                                              ReportList *reports);

/**
 * \return Blend file data, this must be passed to
 * #BKE_blendfile_read_setup_readfile/#BKE_blendfile_read_setup_undo when non-NULL.
 *
 * \note `memfile` is the undo buffer.
 */
BlendFileData *BKE_blendfile_read_from_memfile(Main *bmain,
                                               MemFile *memfile,
                                               const BlendFileReadParams *params,
                                               ReportList *reports);
/**
 * Utility to make a file 'empty' used for startup to optionally give an empty file.
 * Handy for tests.
 */
void BKE_blendfile_read_make_empty(bContext *C);

/**
 * Only read the #UserDef from a .blend.
 */
UserDef *BKE_blendfile_userdef_read(const char *filepath, ReportList *reports);
UserDef *BKE_blendfile_userdef_read_from_memory(const void *file_buf,
                                                int file_buf_size,
                                                ReportList *reports);
UserDef *BKE_blendfile_userdef_from_defaults();

/**
 * Only write the #UserDef in a `.blend`.
 * \return success.
 */
bool BKE_blendfile_userdef_write(const char *filepath, ReportList *reports);
/**
 * Only write the #UserDef in a `.blend`, merging with the existing blend file.
 * \return success.
 *
 * \note In the future we should re-evaluate user preferences,
 * possibly splitting out system/hardware specific preferences.
 */
bool BKE_blendfile_userdef_write_app_template(const char *filepath, ReportList *reports);

bool BKE_blendfile_userdef_write_all(ReportList *reports);

WorkspaceConfigFileData *BKE_blendfile_workspace_config_read(const char *filepath,
                                                             const void *file_buf,
                                                             int file_buf_size,
                                                             ReportList *reports);
bool BKE_blendfile_workspace_config_write(Main *bmain, const char *filepath, ReportList *reports);
void BKE_blendfile_workspace_config_data_free(WorkspaceConfigFileData *workspace_config);

namespace blender::bke::blendfile {

/* Partial blend file writing. */
class PartialWriteContext : public Main {
  /**
   * This mapping only contains entries for IDs in the context which have a known matching ID in
   * current G_MAIN.
   *
   * It is used to avoid adding several time a same ID (e.g. as a dependency of several other added
   * IDs).
   */
  IDNameLib_Map *matching_uid_map_;

  /** A mapping from the absolute library paths to the #Library IDs in the context. */
  blender::Map<std::string, Library *> libraries_map_;

  /**
   * In case an explicitely added ID has the same session_uid as an existing one in current
   * context, the added one should be able to 'steal' that session_uid in the context, and
   * re-assign a new one to the other ID.
   */
  void preempt_session_uid_(ID *ctx_id, unsigned int session_uid);
  /** Utils for #PartialWriteContext::id_add, only adds (duplicate) the given source ID into
   * current context. */
  ID *id_add_copy_(const ID *id, const bool regenerate_session_uid);
  void ensure_library_(ID *ctx_id);

 public:
  PartialWriteContext();
  ~PartialWriteContext();
  /* Delete regular copy constructor, such that only the default move copy constructor (and
   * assignement operator) can be used. */
  PartialWriteContext(const PartialWriteContext &) = delete;
  PartialWriteContext(PartialWriteContext &&) = default;
  PartialWriteContext &operator=(PartialWriteContext &&) = default;

  /**
   * Control how to handle IDs and their dependencies when they are added to this context.
   *
   * \note For linked IDs, if #MAKE_LOCAL is not used, the library ID opinter is _not_ considered
   * nor hanlded as a regular dependency. Instead, the library is _always_ added to the context
   * data, and never duplicated. Also, library matching always happens based on absolute filepath.
   */
  enum AddIDOptions {
    /**
     * Do not keep linked info (library and/or liboverride references).
     *
     * \warning By default, when #ADD_DEPENDENCIES is defined, this will also apply to all
     * dependencies as well.
     */
    MAKE_LOCAL = 1 << 0,
    /**
     * Clear all dependency IDs that are not in the partial write context. Mutually exclusive with
     * #ADD_DEPENDENCIES.
     *
     * WARNING: This also means that dependencies like obdata, shapekeys or actions are not
     * duplicated either.
     */
    CLEAR_DEPENDENCIES = 1 << 8,
    /**
     * Also add (or reuse if already there) dependency IDs into the partial write context. Mutually
     * exclusive with #CLEAR_DEPENDENCIES.
     */
    ADD_DEPENDENCIES = 1 << 9,
    /**
     * For each explicitely added IDs (i.e. these with a fake user), ensure all of their
     * dependencies are independant copies, instead of being shared with other explicitely added
     * IDs. Only relevant with #ADD_DEPENDENCIES.
     *
     * \warning Implies that the `session_uid` of these duplicated dependencies will be different
     * than their source data.
     */
    DUPLICATE_DEPENDENCIES = 1 << 10,
  };
  /**
   * Add a copy of the given ID to the partial write context.
   *
   * \note The duplicated ID will have the same session_uid as its source. In case a matching ID
   * already exists in the context, it is returned instead of duplicating it again.
   *
   * \param options: Control how the added ID (and its dependencies) are handled. See
   *                 #PartialWriteContext::AddIDOptions above for details.
   * \param dependencies_filter_cb optional, a callback called for each ID usages. Currently, only
   * accepted return values are #MAKE_LOCAL, and #ADD_DEPENDENCIES or #CLEAR_DEPENDENCIES.
   *
   * \return The pointer to the duplicated ID in the partial write context.
   */
  ID *id_add(const ID *id,
             PartialWriteContext::AddIDOptions options,
             blender::FunctionRef<PartialWriteContext::AddIDOptions(
                 LibraryIDLinkCallbackData *cb_data, PartialWriteContext::AddIDOptions options)>
                 dependencies_filter_cb = nullptr);

  /**
   * Remove the copy of the given ID from the partial write context.
   *
   * \note The search is based on the #ID.session_uid of the given ID. This means that if
   * `duplicate_depencies` option was used when adding the ID, these independant dependencies
   * duplicates cannot be removed directly from the context. Use #remove_unused for this.
   *
   * \note No dependencies will be removed. Use #remove_unused to remove all unused IDs from the
   * current context.
   */
  void id_remove(const ID *id);

  /**
   * Remove all unused IDs from the current context.
   */
  void remove_unused(void);

  /**
   * Fully empty the partial write context.
   */
  void clear(void);

  /**
   * Debug: Check if the current partial write context is fully valid.
   *
   * Currently, check if any ID in the context still has relations to IDs not in the context.
   *
   * \return false if the context is invalid.
   */
  bool is_valid(void);

  bool write(const char *filepath, int write_flags, int remap_mode, ReportList &reports);
  bool write(const char *filepath, ReportList &reports);
};

}  // namespace blender::bke::blendfile

void BKE_blendfile_write_partial_tag_ID(ID *id, bool set);
void BKE_blendfile_write_partial_begin(Main *bmain_src);
/**
 * \param remap_mode: Choose the kind of path remapping or none #eBLO_WritePathRemap.
 * \return Success.
 */
bool BKE_blendfile_write_partial(
    Main *bmain_src, const char *filepath, int write_flags, int remap_mode, ReportList *reports);
void BKE_blendfile_write_partial_end(Main *bmain_src);
