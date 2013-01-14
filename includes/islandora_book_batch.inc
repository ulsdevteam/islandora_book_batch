<?php

class IslandoraBookBatch extends IslandoraScanBatch {
  /**
   * @overrides IslandoraScanBatch::group_files()
   */
  protected function group_files($files) {
    // TODO: Regroup files into a logical structure...  Basename of a XML/MRC
    //   plus numeric suffixes for page numbers.
  }
}

class IslandoraBookBookBatchObject extends IslandoraScanBatchObject {
  protected function get_children() {
    // TODO: Create page objects.
  }
}

class IslandoraBookPageBatchObject extends IslandoraScanBatchObject {

}

